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
    /* Measurement plan per mode over one 10-round cycle. */
    unsigned co2=0, pm=0;
    for(uint8_t r=1;r<=POWER_ROUNDS;r++){uint8_t m=PowerPolicy_Measurements(POWER_NORMAL,r);co2+=!!(m&POWER_MEASURE_CO2);pm+=!!(m&POWER_MEASURE_PM);}
    assert(co2==2 && pm==1);
    assert(PowerPolicy_Measurements(POWER_NORMAL,5)==POWER_MEASURE_CO2);
    assert(PowerPolicy_Measurements(POWER_NORMAL,10)==(POWER_MEASURE_CO2|POWER_MEASURE_PM));
    assert(PowerPolicy_Measurements(POWER_SAVE,5)==0 && PowerPolicy_Measurements(POWER_SAVE,10)==POWER_MEASURE_CO2);
    for(uint8_t r=0;r<=11;r++) assert(PowerPolicy_Measurements(POWER_RECOVERY,r)==0);
    assert(PowerPolicy_Measurements(POWER_NORMAL,0)==0 && PowerPolicy_Measurements(POWER_NORMAL,11)==0);
    /* 180 s / 360 s: CO2 every 15 min and PM every 30 min, SAVE CO2 hourly. */
    assert(PowerPolicy_Interval(POWER_NORMAL,180)==180 && PowerPolicy_Interval(POWER_SAVE,180)==360);
    puts("Power: low voltage, missing readings, hysteresis, restart stability, seconds wrap and adaptive measurement plan passed");
}
