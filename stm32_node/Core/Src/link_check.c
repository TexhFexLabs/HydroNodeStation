#include "link_check.h"

uint8_t LinkCheck_Joined(link_check_t *lc, uint32_t now)
{
    uint8_t days = lc->rejoining ? lc->missed : 0U;
    *lc = (link_check_t){ .due_at = now + LINK_CHECK_INTERVAL_S };
    return days;
}

bool LinkCheck_Due(const link_check_t *lc, uint32_t now)
{
    return !lc->rejoining && (int32_t)(now - lc->due_at) >= 0;
}

void LinkCheck_Requested(link_check_t *lc, uint32_t now)
{
    lc->due_at = now + LINK_CHECK_INTERVAL_S;
}

bool LinkCheck_Result(link_check_t *lc, bool answered)
{
    if (lc->rejoining) { return false; }
    if (answered) { lc->missed = 0U; return false; }
    if (lc->missed < UINT8_MAX) { lc->missed++; }
    if (lc->missed < LINK_CHECK_REJOIN_DAYS) { return false; }
    lc->rejoining = true;
    return true;
}
