#include "fault_report.h"

static fault_entry_t queue[FAULT_QUEUE_LEN];
static uint8_t head, count;
static uint16_t dropped;
static uint8_t frame_entries;
static bool frame_has_marker;
static uint64_t active[3];   /* categories 0x01..0x03, bit = id & 0x3F */
static uint8_t consecutive[FAULT_COMPONENTS];
static uint16_t failures[FAULT_COMPONENTS];
static uint32_t now;
static uint32_t last_frame;      /* valid with frame_sent */
static bool frame_sent;
static uint32_t last_open[FAULT_COMPONENTS];
static uint32_t good_since[FAULT_COMPONENTS];
static uint32_t opened_mask, flapping_mask, good_mask;   /* bit = component */
static uint16_t counted[2];      /* 0x0431, 0x0432 */
static uint32_t counter_start;
static bool clock_set;

static fault_entry_t *at(uint8_t i) { return &queue[(uint8_t)(head + i) % FAULT_QUEUE_LEN]; }

static void drop_at(uint8_t i)
{
    for (; i + 1U < count; ++i) { *at(i) = *at((uint8_t)(i + 1U)); }
    count--;
    if (dropped < UINT16_MAX) { dropped++; }
}

static void push(uint16_t code, uint16_t detail)
{
    if (count == FAULT_QUEUE_LEN)
    {
        /* Full: the oldest "resolved" entry goes first, else the oldest.
         * The entries of a frame in flight are kept. */
        uint8_t victim = frame_entries;
        for (uint8_t i = frame_entries; i < count; ++i)
        {
            if ((at(i)->code & FAULT_RESOLVED) != 0U) { victim = i; break; }
        }
        drop_at(victim);
    }
    *at(count) = (fault_entry_t){ code, detail };
    count++;
}

static uint64_t *state_word(uint16_t code, uint64_t *bit)
{
    uint8_t cat = FAULT_CATEGORY(code);
    if (cat < FAULT_CAT_NOT_FOUND || cat > FAULT_CAT_SUPPLY) { return 0; }
    *bit = 1ULL << (code & 0x3FU);
    return &active[cat - 1U];
}

void Fault_SetState(uint16_t code, bool on, uint16_t detail)
{
    uint64_t bit;
    uint64_t *word = state_word((uint16_t)(code & ~FAULT_RESOLVED), &bit);
    if (word == 0) { return; }
    if (on == ((*word & bit) != 0U)) { return; }
    if (on) { *word |= bit; push((uint16_t)(code & ~FAULT_RESOLVED), detail); }
    else { *word &= ~bit; push((uint16_t)(code | FAULT_RESOLVED), detail); }
}

bool Fault_IsActive(uint16_t code)
{
    uint64_t bit;
    uint64_t *word = state_word((uint16_t)(code & ~FAULT_RESOLVED), &bit);
    return word != 0 && (*word & bit) != 0U;
}

void Fault_Event(uint16_t code, uint16_t detail)
{
    push(code, detail);
}

void Fault_Count(uint16_t code)
{
    uint8_t i;
    if (code == FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_FAILED)) { i = 0U; }
    else if (code == FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_REJECTED)) { i = 1U; }
    else { return; }
    if (counted[i] < UINT16_MAX) { counted[i]++; }
}

void Fault_Tick(uint32_t now_s)
{
    now = now_s;
    if (!clock_set) { clock_set = true; counter_start = now_s; return; }
    if ((uint32_t)(now_s - counter_start) < FAULT_COUNTER_PERIOD_S) { return; }
    counter_start = now_s;
    if (counted[0] != 0U) { push(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_FAILED), counted[0]); }
    if (counted[1] != 0U) { push(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_REJECTED), counted[1]); }
    counted[0] = counted[1] = 0U;
}

bool Fault_IsUrgent(uint16_t code)
{
    uint16_t base = (uint16_t)(code & ~FAULT_RESOLVED);
    switch (FAULT_CATEGORY(code))
    {
        case FAULT_CAT_NOT_FOUND:
        case FAULT_CAT_SUPPLY:
        case FAULT_CAT_COMMAND:
            return true;
        default:
            break;
    }
    return base == FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_BOOT) ||
           base == FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_VERSION) ||
           base == FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_LSE_RETRY) ||
           base == FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_NO_IWDG_STDBY) ||
           base == FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_REJOIN);
}

bool Fault_FrameDue(void)
{
    if (Fault_Pending() == 0U) { return false; }
    for (uint8_t i = 0U; i < count; ++i)
    {
        if (Fault_IsUrgent(at(i)->code)) { return true; }
    }
    return !frame_sent || (uint32_t)(now - last_frame) >= FAULT_NORMAL_INTERVAL_S;
}

void Fault_ComponentInit(uint8_t comp, bool found, uint8_t i2c_addr)
{
    if (comp >= FAULT_COMPONENTS) { return; }
    consecutive[comp] = 0U;
    failures[comp] = 0U;
    Fault_SetState(FAULT_CODE(FAULT_CAT_NOT_FOUND, comp), !found, found ? 0U : (uint16_t)(i2c_addr & 0x7FU));
}

void Fault_ComponentResult(uint8_t comp, bool ok, uint32_t i2c_error)
{
    if (comp >= FAULT_COMPONENTS) { return; }
    uint16_t missing = FAULT_CODE(FAULT_CAT_NOT_FOUND, comp);
    uint16_t failing = FAULT_CODE(FAULT_CAT_READ_ERROR, comp);
    uint32_t bit = 1UL << comp;
    if (ok)
    {
        Fault_SetState(missing, false, 0U);
        consecutive[comp] = 0U;
        if (!Fault_IsActive(failing)) { failures[comp] = 0U; return; }
        if ((flapping_mask & bit) != 0U)
        {
            /* Flapping: report "resolved" only after a quiet hold time. */
            if ((good_mask & bit) == 0U) { good_mask |= bit; good_since[comp] = now; }
            if ((uint32_t)(now - good_since[comp]) < FAULT_FLAP_HOLD_S) { return; }
        }
        Fault_SetState(failing, false, failures[comp]);
        flapping_mask &= ~bit;
        good_mask &= ~bit;
        failures[comp] = 0U;
        return;
    }
    good_mask &= ~bit;
    if (failures[comp] < UINT16_MAX) { failures[comp]++; }
    if (consecutive[comp] < UINT8_MAX) { consecutive[comp]++; }
    /* A part that was never found is already reported as missing. */
    if (consecutive[comp] >= FAULT_READ_ERROR_LIMIT && !Fault_IsActive(missing) && !Fault_IsActive(failing))
    {
        if ((opened_mask & bit) != 0U && (uint32_t)(now - last_open[comp]) < FAULT_FLAP_HOLD_S)
        {
            flapping_mask |= bit;
        }
        opened_mask |= bit;
        last_open[comp] = now;
        Fault_SetState(failing, true, (uint16_t)i2c_error);
    }
}

static void put16(uint8_t *buf, uint8_t *n, uint16_t v)
{
    buf[(*n)++] = (uint8_t)(v >> 8);
    buf[(*n)++] = (uint8_t)v;
}

uint8_t Fault_BuildFrame(uint8_t *buf)
{
    uint8_t n = 0U;
    frame_entries = 0U;
    frame_has_marker = dropped != 0U;
    if (frame_has_marker)
    {
        put16(buf, &n, FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_OVERFLOW));
        put16(buf, &n, dropped);
    }
    while (frame_entries < count && n < FAULT_FRAME_MAX_LEN)
    {
        put16(buf, &n, at(frame_entries)->code);
        put16(buf, &n, at(frame_entries)->detail);
        frame_entries++;
    }
    return n;
}

void Fault_FrameAccepted(void)
{
    head = (uint8_t)((head + frame_entries) % FAULT_QUEUE_LEN);
    count = (uint8_t)(count - frame_entries);
    frame_entries = 0U;
    if (frame_has_marker) { dropped = 0U; }
    frame_has_marker = false;
    frame_sent = true;
    last_frame = now;
}

uint8_t Fault_Pending(void)
{
    return (uint8_t)(count + (dropped != 0U ? 1U : 0U));
}

uint8_t Fault_CompressResetFlags(uint32_t csr)
{
    uint8_t f = 0U;
    if (csr & (1UL << 26)) { f |= FAULT_RST_PIN; }
    if (csr & (1UL << 27)) { f |= FAULT_RST_BOR; }
    if (csr & (1UL << 28)) { f |= FAULT_RST_SOFTWARE; }
    if (csr & (1UL << 29)) { f |= FAULT_RST_IWDG; }
    if (csr & (1UL << 30)) { f |= FAULT_RST_WWDG; }
    if (csr & (1UL << 31)) { f |= FAULT_RST_LOW_POWER; }
    if (csr & (1UL << 25)) { f |= FAULT_RST_OPTION_BYTE; }
    return f;
}

void Fault_Reset(void)
{
    head = count = frame_entries = 0U;
    dropped = 0U;
    frame_has_marker = false;
    active[0] = active[1] = active[2] = 0U;
    for (uint8_t i = 0U; i < FAULT_COMPONENTS; ++i) { consecutive[i] = 0U; failures[i] = 0U; }
    now = last_frame = counter_start = 0U;
    frame_sent = clock_set = false;
    opened_mask = flapping_mask = good_mask = 0U;
    counted[0] = counted[1] = 0U;
}
