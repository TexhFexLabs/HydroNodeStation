#include "fault_report.h"
#include <assert.h>
#include <stdio.h>

static uint16_t be(const uint8_t *b, unsigned i) { return (uint16_t)(b[i] << 8 | b[i + 1]); }

int main(void)
{
    uint8_t f[FAULT_FRAME_MAX_LEN];
    Fault_Reset();
    assert(Fault_BuildFrame(f) == 0U);

    /* Boot frame: 0x0540 (no fault, pin + BOR reset) and 0x0541 (2.0). */
    Fault_Event(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_BOOT),
                (uint16_t)(0U << 8 | Fault_CompressResetFlags((1UL << 26) | (1UL << 27))));
    Fault_Event(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_VERSION), 0x0200U);
    assert(Fault_BuildFrame(f) == 8U);
    assert(be(f, 0) == 0x0540 && be(f, 2) == 0x0003 && be(f, 4) == 0x0541 && be(f, 6) == 0x0200);
    /* Not accepted (duty cycle): the same frame comes again. */
    assert(Fault_BuildFrame(f) == 8U && be(f, 0) == 0x0540);
    Fault_FrameAccepted();
    assert(Fault_Pending() == 0U);

    /* BMP390 missing at boot, found later: 0x0102/0x0077, then 0x8102/0. */
    Fault_ComponentInit(FAULT_COMP_BMP390, false, 0x77);
    for (int i = 0; i < 5; i++) Fault_ComponentResult(FAULT_COMP_BMP390, false, 4U);  /* no 0x0202 */
    Fault_ComponentResult(FAULT_COMP_BMP390, true, 0U);
    assert(Fault_BuildFrame(f) == 8U);
    assert(be(f, 0) == 0x0102 && be(f, 2) == 0x0077 && be(f, 4) == 0x8102 && be(f, 6) == 0);
    Fault_FrameAccepted();

    /* SPS30 read errors: 2 in a row nothing, the 3rd raises 0x0205 with the
     * HAL error bits, more add nothing, recovery reports the attempts. */
    Fault_ComponentInit(FAULT_COMP_SPS30, true, 0x69);
    Fault_ComponentResult(FAULT_COMP_SPS30, false, 4U);
    Fault_ComponentResult(FAULT_COMP_SPS30, false, 4U);
    assert(Fault_Pending() == 0U);
    Fault_ComponentResult(FAULT_COMP_SPS30, false, 0x20U);
    for (int i = 0; i < 4; i++) Fault_ComponentResult(FAULT_COMP_SPS30, false, 4U);
    Fault_ComponentResult(FAULT_COMP_SPS30, true, 0U);
    assert(Fault_BuildFrame(f) == 8U);
    assert(be(f, 0) == 0x0205 && be(f, 2) == 0x0020 && be(f, 4) == 0x8205 && be(f, 6) == 7);
    Fault_FrameAccepted();

    /* Supply states: SAVE on, repeated on adds nothing, off reports. */
    Fault_SetState(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_SAVE), true, 3480U);
    Fault_SetState(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_SAVE), true, 3470U);
    Fault_SetState(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_SAVE), false, 3660U);
    assert(Fault_Pending() == 2U);
    /* Events repeat every time; categories 4-6 are no states. */
    Fault_SetState(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_REJOIN), true, 3U);
    assert(Fault_Pending() == 2U);
    Fault_Event(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_REJOIN), 3U);
    Fault_Event(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_REJOIN), 3U);
    assert(Fault_Pending() == 4U);
    /* At most 8 entries per frame, the rest follows. */
    for (int i = 0; i < 6; i++) Fault_Event(FAULT_CODE(FAULT_CAT_COMMAND, 0x10), 0U);
    assert(Fault_Pending() == 10U && Fault_BuildFrame(f) == 32U);
    assert(be(f, 0) == 0x0320 && be(f, 2) == 3480 && be(f, 4) == 0x8320 && be(f, 6) == 3660);
    Fault_FrameAccepted();
    assert(Fault_Pending() == 2U && Fault_BuildFrame(f) == 8U);
    Fault_FrameAccepted();

    /* Overflow: 16 slots. Resolved entries go first, then the oldest; the
     * next frame starts with 0x054F carrying the number dropped. */
    for (int i = 0; i < 8; i++) Fault_Event(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_FAILED), (uint16_t)i);
    Fault_SetState(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_RECOVERY), true, 3200U);
    Fault_SetState(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_RECOVERY), false, 3700U);
    for (int i = 8; i < 14; i++) Fault_Event(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_FAILED), (uint16_t)i);
    assert(Fault_Pending() == 16U);
    Fault_Event(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_NVM), 9U);   /* drops 0x8321 */
    Fault_Event(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_NVM), 9U);   /* drops the oldest 0x0431 */
    assert(Fault_Pending() == 17U);   /* 16 entries + marker */
    assert(Fault_BuildFrame(f) == 32U);
    assert(be(f, 0) == 0x054F && be(f, 2) == 2);
    assert(be(f, 4) == 0x0431 && be(f, 6) == 1);    /* entry 0 is gone */
    Fault_FrameAccepted();
    assert(Fault_Pending() == 9U);
    int saw_recovery_on = 0, saw_recovery_off = 0;
    while (Fault_Pending())
    {
        uint8_t n = Fault_BuildFrame(f);
        for (uint8_t i = 0; i < n; i += 4) { saw_recovery_on |= be(f, i) == 0x0321; saw_recovery_off |= be(f, i) == 0x8321; }
        Fault_FrameAccepted();
    }
    assert(saw_recovery_on && !saw_recovery_off);
    assert(Fault_BuildFrame(f) == 0U);

    /* Reset flags: pin 26, BOR 27, SW 28, IWDG 29, WWDG 30, LPWR 31, OBL 25. */
    assert(Fault_CompressResetFlags(0xFE000000UL) == 0x7F);
    assert(Fault_CompressResetFlags(1UL << 29) == FAULT_RST_IWDG);
    /* Priorities: urgent entries make a frame due at once, the rest waits an
     * hour after the last accepted frame. */
    Fault_Reset();
    Fault_Tick(1000U);
    assert(!Fault_FrameDue());
    Fault_Event(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_BOOT), 1U);
    assert(Fault_IsUrgent(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_BOOT)) && Fault_FrameDue());
    Fault_BuildFrame(f); Fault_FrameAccepted();
    assert(!Fault_IsUrgent(FAULT_CODE(FAULT_CAT_READ_ERROR, FAULT_COMP_SPS30)));
    assert(!Fault_IsUrgent(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_NVM)));
    assert(Fault_IsUrgent(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_SAVE) | FAULT_RESOLVED));
    assert(Fault_IsUrgent(FAULT_CODE(FAULT_CAT_COMMAND, 0x10)) && Fault_IsUrgent(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_REJOIN)));
    Fault_Event(FAULT_CODE(FAULT_CAT_SYSTEM, FAULT_SYS_NVM), 3U);
    Fault_Tick(1000U + 3599U); assert(!Fault_FrameDue());
    Fault_Tick(1000U + 3600U); assert(Fault_FrameDue());
    Fault_Tick(1100U);
    Fault_SetState(FAULT_CODE(FAULT_CAT_SUPPLY, FAULT_SUPPLY_SAVE), true, 3490U);
    assert(Fault_FrameDue());   /* urgent entry, the NVM entry rides along */
    assert(Fault_BuildFrame(f) == 8U && be(f, 0) == 0x0543 && be(f, 4) == 0x0320);
    Fault_FrameAccepted();

    /* Flapping read errors: the first open/close pair goes out as it happens;
     * a second open within 6 h stays open until 6 h of good reads. */
    Fault_Reset();
    uint32_t t = 50000U;
    Fault_Tick(t);
    Fault_ComponentInit(FAULT_COMP_LTR390, true, 0x53);
    for (int i = 0; i < 3; i++) Fault_ComponentResult(FAULT_COMP_LTR390, false, 4U);
    Fault_Tick(t += 600U); Fault_ComponentResult(FAULT_COMP_LTR390, true, 0U);
    assert(Fault_Pending() == 2U && !Fault_IsActive(FAULT_CODE(FAULT_CAT_READ_ERROR, FAULT_COMP_LTR390)));
    Fault_BuildFrame(f); Fault_FrameAccepted();
    Fault_Tick(t += 600U);
    for (int i = 0; i < 3; i++) Fault_ComponentResult(FAULT_COMP_LTR390, false, 4U);
    assert(Fault_Pending() == 1U);   /* second open is reported */
    for (int k = 0; k < 20; k++)     /* then it flaps for hours: nothing more */
    {
        Fault_Tick(t += 600U); Fault_ComponentResult(FAULT_COMP_LTR390, true, 0U);
        Fault_Tick(t += 600U); for (int i = 0; i < 3; i++) Fault_ComponentResult(FAULT_COMP_LTR390, false, 4U);
    }
    assert(Fault_Pending() == 1U && Fault_IsActive(FAULT_CODE(FAULT_CAT_READ_ERROR, FAULT_COMP_LTR390)));
    Fault_Tick(t += 600U); Fault_ComponentResult(FAULT_COMP_LTR390, true, 0U);
    Fault_Tick(t += FAULT_FLAP_HOLD_S - 1U); Fault_ComponentResult(FAULT_COMP_LTR390, true, 0U);
    assert(Fault_Pending() == 1U);
    Fault_Tick(t += 1U); Fault_ComponentResult(FAULT_COMP_LTR390, true, 0U);
    assert(Fault_Pending() == 2U && !Fault_IsActive(FAULT_CODE(FAULT_CAT_READ_ERROR, FAULT_COMP_LTR390)));
    Fault_BuildFrame(f);
    assert(be(f, 4) == 0x8203 && be(f, 6) == 3U + 20U * 3U);   /* all failed reads since the open */
    Fault_FrameAccepted();
    /* Hours later a single new failure-triple is a plain open again. */
    Fault_Tick(t += FAULT_FLAP_HOLD_S);
    for (int i = 0; i < 3; i++) Fault_ComponentResult(FAULT_COMP_LTR390, false, 4U);
    Fault_Tick(t += 60U); Fault_ComponentResult(FAULT_COMP_LTR390, true, 0U);
    assert(Fault_Pending() == 2U);

    /* Counted radio events: one entry per day with the count, none at 0. */
    Fault_Reset();
    Fault_Tick(UINT32_MAX - 100U);   /* across the seconds wrap */
    for (int i = 0; i < 5; i++) Fault_Count(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_TX_FAILED));
    Fault_Count(FAULT_CODE(FAULT_CAT_RADIO, FAULT_RADIO_REJOIN));   /* not counted */
    assert(Fault_Pending() == 0U);
    Fault_Tick(UINT32_MAX - 100U + FAULT_COUNTER_PERIOD_S - 1U); assert(Fault_Pending() == 0U);
    Fault_Tick(UINT32_MAX - 100U + FAULT_COUNTER_PERIOD_S);
    assert(Fault_Pending() == 1U && Fault_BuildFrame(f) == 4U && be(f, 0) == 0x0431 && be(f, 2) == 5U);
    Fault_FrameAccepted();
    Fault_Tick(UINT32_MAX - 100U + 2U * FAULT_COUNTER_PERIOD_S); assert(Fault_Pending() == 0U);

    puts("Fault report: boot frame, state edges, 3-in-a-row read errors, 8-entry frames, overflow policy, reset flags, priorities, flap damping and daily counters passed");
    return 0;
}
