#include "link_check.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    link_check_t lc;
    uint32_t now = UINT32_MAX - 1000U;   /* across the 32-bit seconds wrap */
    assert(LinkCheck_Joined(&lc, now) == 0U);
    assert(!LinkCheck_Due(&lc, now + LINK_CHECK_INTERVAL_S - 1U));
    assert(LinkCheck_Due(&lc, now + LINK_CHECK_INTERVAL_S));
    now += LINK_CHECK_INTERVAL_S; LinkCheck_Requested(&lc, now);
    assert(!LinkCheck_Due(&lc, now + 1U));
    /* Two misses then an answer: no rejoin, counter cleared. */
    assert(!LinkCheck_Result(&lc, false) && !LinkCheck_Result(&lc, false));
    assert(!LinkCheck_Result(&lc, true) && lc.missed == 0U);
    /* Three misses in a row: rejoin once, no further checks meanwhile. */
    assert(!LinkCheck_Result(&lc, false) && !LinkCheck_Result(&lc, false));
    assert(LinkCheck_Result(&lc, false));
    assert(!LinkCheck_Due(&lc, now + 10U * LINK_CHECK_INTERVAL_S));
    assert(!LinkCheck_Result(&lc, false));
    /* The join after the rejoin reports the days, an ordinary join does not. */
    assert(LinkCheck_Joined(&lc, now) == 3U);
    assert(LinkCheck_Joined(&lc, now) == 0U && lc.missed == 0U);
    puts("Link check: daily schedule over the seconds wrap, three-day rejoin and rejoin report passed");
    return 0;
}
