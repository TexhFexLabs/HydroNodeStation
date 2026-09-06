#include "power_policy.h"
void PowerPolicy_Init(power_policy_t *p, uint16_t mv, bool valid)
{
    *p = (power_policy_t){.mode = (valid && mv >= POWER_RESTART_MV) ? POWER_NORMAL : POWER_RECOVERY};
}
void PowerPolicy_Update(power_policy_t *p, uint16_t mv, bool valid, uint32_t now)
{
    if (!valid)
    {
        p->stable = 0;
        if (p->failures < 3U) { p->failures++; }
        if (p->failures >= 3U) { p->mode = POWER_RECOVERY; }
        return;
    }
    p->failures = 0;
    if (mv < POWER_STOP_MV) { p->mode = POWER_RECOVERY; p->stable = 0; return; }
    if (p->mode == POWER_RECOVERY)
    {
        if (mv < POWER_RESTART_MV) { p->stable = 0; return; }
        if (!p->stable) { p->stable = 1; p->recovery_since = now; }
        if ((uint32_t)(now - p->recovery_since) >= POWER_RESTART_STABLE_S)
        { p->mode = mv >= POWER_NORMAL_MV ? POWER_NORMAL : POWER_SAVE; }
    }
    else if (mv < POWER_SAVE_MV) { p->mode = POWER_SAVE; }
    else if (mv >= POWER_NORMAL_MV) { p->mode = POWER_NORMAL; }
}
