#include "power_policy.h"
#include <assert.h>
#include <stdio.h>
static const power_thresholds_t DEF = POWER_DEFAULT_THRESHOLDS;

int main(void)
{
    power_policy_t p;
    PowerPolicy_Init(&p,&DEF,4200,true); assert(p.mode==POWER_NORMAL);
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
    PowerPolicy_Init(&p,&DEF,65535,false); assert(p.mode==POWER_RECOVERY);
    PowerPolicy_Update(&p,3700,true,UINT32_MAX-30);
    PowerPolicy_Update(&p,3700,true,30); assert(p.mode==POWER_NORMAL);
    /* Standby: two valid readings below 3200 mV in a row, invalid ones never. */
    PowerPolicy_Init(&p,&DEF,3700,true);
    PowerPolicy_Update(&p,3190,true,1); assert(p.mode==POWER_RECOVERY && !p.standby);
    PowerPolicy_Update(&p,65535,false,2); assert(!p.standby);
    PowerPolicy_Update(&p,3190,true,3); assert(!p.standby);
    PowerPolicy_Update(&p,3210,true,4); assert(!p.standby);
    PowerPolicy_Update(&p,3180,true,5); assert(!p.standby);
    PowerPolicy_Update(&p,3170,true,6); assert(p.standby && p.mode==POWER_RECOVERY);
    for(int i=0;i<5;i++) PowerPolicy_Update(&p,65535,false,7+i);
    assert(!p.standby);
    PowerPolicy_Update(&p,3300,true,20); PowerPolicy_Update(&p,3300,true,21); assert(!p.standby);
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
    /* Own thresholds (LiFePO4-like): every comparison follows them. */
    const power_thresholds_t own = {3100, 3000, 2800, 3200};
    assert(PowerPolicy_Validate(&own) && PowerPolicy_Validate(&DEF));
    PowerPolicy_Init(&p,&own,3150,true); assert(p.mode==POWER_RECOVERY);   /* boot needs Resume */
    PowerPolicy_Init(&p,&own,3200,true); assert(p.mode==POWER_NORMAL);
    PowerPolicy_Update(&p,3099,true,1); assert(p.mode==POWER_SAVE);
    PowerPolicy_Update(&p,3249,true,2); assert(p.mode==POWER_SAVE);        /* exit is Save + 150 */
    PowerPolicy_Update(&p,3250,true,3); assert(p.mode==POWER_NORMAL);
    PowerPolicy_Update(&p,2999,true,4); assert(p.mode==POWER_RECOVERY && !p.standby);
    PowerPolicy_Update(&p,2799,true,5); PowerPolicy_Update(&p,2799,true,6); assert(p.standby);
    PowerPolicy_Update(&p,3200,true,10); PowerPolicy_Update(&p,3200,true,70);
    assert(p.mode==POWER_SAVE);                                            /* resumed below Save + 150 */
    /* New thresholds apply from the next reading. */
    PowerPolicy_Init(&p,&DEF,3700,true);
    PowerPolicy_SetThresholds(&p,&own); assert(p.mode==POWER_NORMAL);
    PowerPolicy_Update(&p,3400,true,1); assert(p.mode==POWER_NORMAL);
    /* Rules of Power-Sync §5 on pack mV. */
    assert(!PowerPolicy_Validate(&(power_thresholds_t){3500,3300,3251,3600}));  /* standby too close */
    assert(!PowerPolicy_Validate(&(power_thresholds_t){3349,3300,3200,3600}));  /* recovery too close */
    assert(!PowerPolicy_Validate(&(power_thresholds_t){3500,3300,3200,3399}));  /* resume too close */
    assert(!PowerPolicy_Validate(&(power_thresholds_t){3500,3300,3200,3901}));  /* resume above save + 400 */
    assert(PowerPolicy_Validate(&(power_thresholds_t){3500,3300,3200,3900}));
    assert(PowerPolicy_Validate(&(power_thresholds_t){3350,3300,3250,3400}));   /* smallest gaps */
    assert(!PowerPolicy_Validate(&(power_thresholds_t){3500,3300,2799,3600}));  /* below 2800 */
    assert(!PowerPolicy_Validate(&(power_thresholds_t){4201,3300,3200,3800}));  /* above 4200 */
    /* Resume through Standby: tagged backup word, default for anything else. */
    assert(PowerPolicy_UnpackResume(PowerPolicy_PackResume(3550))==3550);
    assert(PowerPolicy_UnpackResume(0U)==POWER_DEFAULT_RESUME_MV);
    assert(PowerPolicy_UnpackResume(3550U)==POWER_DEFAULT_RESUME_MV);
    assert(PowerPolicy_UnpackResume(PowerPolicy_PackResume(9000))==POWER_DEFAULT_RESUME_MV);
    puts("Power: own thresholds, save hysteresis at Save + 150, validation rules and Standby resume word passed");
    puts("Power: low voltage, missing readings, hysteresis, restart stability, seconds wrap, adaptive measurement plan and standby entry passed");
}
