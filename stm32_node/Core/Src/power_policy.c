#include "power_policy.h"

#define RESUME_TAG 0x52530000UL /* "RS" in the upper half of the backup word */

bool PowerPolicy_Validate(const power_thresholds_t *t)
{
    const uint16_t v[] = {t->save, t->recovery, t->standby, t->resume};
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; ++i)
    {
        if (v[i] < POWER_PACK_MIN_MV || v[i] > POWER_PACK_MAX_MV) return false;
    }
    const uint32_t save = t->save, recovery = t->recovery, standby = t->standby, resume = t->resume;
    return standby + POWER_GAP_STANDBY_MV <= recovery &&
           recovery + POWER_GAP_RECOVERY_MV <= save &&
           recovery + POWER_GAP_RESUME_MV <= resume &&
           resume <= save + POWER_RESUME_ABOVE_SAVE_MV;
}

void PowerPolicy_Init(power_policy_t *p, const power_thresholds_t *t, uint16_t mv, bool valid)
{
    *p = (power_policy_t){.th = *t,
                          .mode = (valid && mv >= t->resume) ? POWER_NORMAL : POWER_RECOVERY};
}

void PowerPolicy_SetThresholds(power_policy_t *p, const power_thresholds_t *t)
{
    p->th = *t;
}

void PowerPolicy_Update(power_policy_t *p, uint16_t mv, bool valid, uint32_t now)
{
    if (!valid)
    {
        /* A missing reading is no undervoltage: it never leads to Standby. */
        p->low = 0;
        p->standby = false;
        p->stable = 0;
        if (p->failures < 3U) { p->failures++; }
        if (p->failures >= 3U) { p->mode = POWER_RECOVERY; }
        return;
    }
    p->failures = 0;
    if (mv < p->th.recovery)
    {
        p->mode = POWER_RECOVERY;
        p->stable = 0;
        if (mv < p->th.standby) { if (p->low < 2U) { p->low++; } }
        else { p->low = 0; }
        p->standby = p->low >= 2U;
        return;
    }
    p->low = 0;
    p->standby = false;
    uint32_t save_exit = (uint32_t)p->th.save + POWER_SAVE_EXIT_MV;
    if (p->mode == POWER_RECOVERY)
    {
        if (mv < p->th.resume) { p->stable = 0; return; }
        if (!p->stable) { p->stable = 1; p->recovery_since = now; }
        if ((uint32_t)(now - p->recovery_since) >= POWER_RESTART_STABLE_S)
        { p->mode = mv >= save_exit ? POWER_NORMAL : POWER_SAVE; }
    }
    else if (mv < p->th.save) { p->mode = POWER_SAVE; }
    else if (mv >= save_exit) { p->mode = POWER_NORMAL; }
}

uint32_t PowerPolicy_PackResume(uint16_t resume_mv)
{
    return RESUME_TAG | resume_mv;
}

uint16_t PowerPolicy_UnpackResume(uint32_t word)
{
    uint16_t mv = (uint16_t)word;
    if ((word & 0xFFFF0000UL) != RESUME_TAG || mv < POWER_PACK_MIN_MV || mv > POWER_PACK_MAX_MV)
        return POWER_DEFAULT_RESUME_MV;
    return mv;
}

uint8_t PowerPolicy_Measurements(power_mode_t mode, uint8_t round)
{
    if (round == 0U || round > POWER_ROUNDS) { return 0U; }
    switch (mode)
    {
        case POWER_NORMAL:
            return (uint8_t)(((round % 5U == 0U) ? POWER_MEASURE_CO2 : 0U) |
                             ((round % 10U == 0U) ? POWER_MEASURE_PM : 0U));
        case POWER_SAVE:
            return (round % 10U == 0U) ? POWER_MEASURE_CO2 : 0U;
        default:
            return 0U;
    }
}

uint32_t PowerPolicy_Interval(power_mode_t mode, uint32_t base_s)
{
    return (mode == POWER_SAVE) ? 2U * base_s : base_s;
}
