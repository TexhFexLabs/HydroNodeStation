#include "pulse_counter.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    pulse_counter_t pc = { 0 };
    uint32_t t = UINT32_MAX - 30U;   /* across the millisecond wrap */
    /* A bucket tip bounces for a few ms: one count. */
    assert(PulseCounter_Edge(&pc, 0, t, 50U));
    assert(!PulseCounter_Edge(&pc, 0, t + 3U, 50U));
    assert(!PulseCounter_Edge(&pc, 0, t + 49U, 50U));
    assert(PulseCounter_Edge(&pc, 0, t + 50U, 50U));
    assert(PulseCounter_Since(&pc, 0) == 2U);
    /* Channel 2 with 2 ms lockout: 40 Hz contact counts every edge. */
    for (uint32_t i = 0; i < 40U; i++) assert(PulseCounter_Edge(&pc, 1, i * 25U, 2U));
    assert(!PulseCounter_Edge(&pc, 1, 39U * 25U + 1U, 2U));
    assert(PulseCounter_Since(&pc, 1) == 40U);
    /* A block carried 40; 5 more arrive before the modem accepts it. */
    uint16_t sent = PulseCounter_Since(&pc, 1);
    for (uint32_t i = 40; i < 45U; i++) (void)PulseCounter_Edge(&pc, 1, i * 25U, 2U);
    PulseCounter_Advance(&pc, 1, sent);
    assert(PulseCounter_Since(&pc, 1) == 5U);
    /* Saturation: more than 0xFFFE pulses report 0xFFFE and drop the excess. */
    pc.total[0] = pc.committed[0] + 70000U;
    sent = PulseCounter_Since(&pc, 0);
    assert(sent == 0xFFFEU);
    PulseCounter_Advance(&pc, 0, sent);
    assert(PulseCounter_Since(&pc, 0) == 0U);
    /* Counters not built in: both missing. */
    uint16_t a, b;
    PulseCounter_Read(&a, &b);
    assert(a == 0xFFFFU && b == 0xFFFFU);
    puts("Pulse counter: lockout over the ms wrap, per-channel counts, carry-over until accepted, saturation, off = 0xFFFF passed");
    return 0;
}
