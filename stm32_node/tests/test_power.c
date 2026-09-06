#include "power_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    power_policy_t p;
    PowerPolicy_Init(&p,4200,true); assert(p.mode==POWER_NORMAL);
    PowerPolicy_Update(&p,3490,true,10); assert(p.mode==POWER_SAVE);
    PowerPolicy_Update(&p,3550,true,20); assert(p.mode==POWER_SAVE);
    PowerPolicy_Update(&p,3650,true,30); assert(p.mode==POWER_NORMAL);
    PowerPolicy_Update(&p,3290,true,40); assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Update(&p,3700,true,50); assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Update(&p,3590,true,100); assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Update(&p,3700,true,110);
    PowerPolicy_Update(&p,3700,true,169); assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Update(&p,3700,true,170); assert(p.mode==POWER_NORMAL);
    for(int i=0;i<3;i++) PowerPolicy_Update(&p,65535,false,180+i);
    assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Init(&p,65535,false); assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Update(&p,3700,true,UINT32_MAX-30);
    PowerPolicy_Update(&p,3700,true,30); assert(p.mode==POWER_NORMAL);
    puts("Power: low voltage, missing readings, hysteresis, restart stability and seconds wrap passed");
}
