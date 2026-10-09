#!/usr/bin/env python3
"""Host regression tests. No STM32 hardware or third-party Python packages needed."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
import test_sps30
import test_scd41
import test_health
ROOT = Path(__file__).resolve().parents[1]

def function(path, signature):
    text = (ROOT / path).read_text()
    start = text.index(signature + '\n{')
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def compile_run(directory, name, files):
    exe = directory / name
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'Core/Inc'),
                    *map(str,files), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

with tempfile.TemporaryDirectory(prefix='hydronode-tests-') as temp:
    tmp = Path(temp)
    test_health.run(ROOT, tmp)
    test_sps30.run(ROOT, tmp)
    test_scd41.run(ROOT, tmp)
    compile_run(tmp, 'power', [ROOT/'tests/test_power.c', ROOT/'Core/Src/power_policy.c'])
    compile_run(tmp, 'nvm', [ROOT/'tests/test_nvm.c', ROOT/'Core/Src/nvm_store.c'])
    compile_run(tmp, 'solar', [ROOT/'tests/test_solar.c', ROOT/'Core/Src/solar.c'])
    compile_run(tmp, 'pulse', [ROOT/'tests/test_pulse_counter.c', ROOT/'Core/Src/pulse_counter.c'])
    compile_run(tmp, 'payload', [ROOT/'tests/test_payload.c', ROOT/'Core/Src/payload.c'])
    compile_run(tmp, 'settings', [ROOT/'tests/test_device_settings.c', ROOT/'Core/Src/device_settings.c',
                                  ROOT/'Core/Src/power_policy.c'])
    # Same rule table as the backend (tests/vectors, Power-Sync §5): 1S LiPo rows only. The station
    # accepts 2800 to 4200 mV for every chemistry, so rows above 4100 mV (LiPo cell range) are left out.
    rules = json.loads((ROOT/'tests/vectors/threshold-rules-vectors.json').read_text())['cases']
    rows = [c for c in rules if c['cells'] == 1 and c['chemistry'] in (None, 'LIPO')
            and max(c['save'], c['recovery'], c['standby'], c['resume']) <= 4100]
    (tmp/'rules.c').write_text('#include "power_policy.h"\n#include <assert.h>\n#include <stdio.h>\nint main(void) {\n' +
        ''.join(f"    assert(PowerPolicy_Validate(&(power_thresholds_t){{{c['save']},{c['recovery']},{c['standby']},{c['resume']}}}) == {int(not c['expected'])}); /* {c['name']} */\n" for c in rows) +
        f'    puts("Power rules: {len(rows)} rows of the shared backend table passed");\n}}\n')
    compile_run(tmp, 'rules', [tmp/'rules.c', ROOT/'Core/Src/power_policy.c'])
    compile_run(tmp, 'faults', [ROOT/'tests/test_fault_report.c', ROOT/'Core/Src/fault_report.c'])
    compile_run(tmp, 'linkcheck', [ROOT/'tests/test_link_check.c', ROOT/'Core/Src/link_check.c'])
    gauge = function('Core/Src/max17048.c', 'static uint16_t max17048_soc_x100(uint16_t raw)') + '\n' + \
            function('Core/Src/max17048.c', 'static int16_t max17048_crate_x100(uint16_t raw)')
    (tmp/'gauge.c').write_text(r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
''' + gauge + r'''
int main(void) {
    assert(max17048_soc_x100(0x6400) == 10000);   /* 100 % */
    assert(max17048_soc_x100(0x3280) == 5050);    /* 50.5 % */
    assert(max17048_soc_x100(0xFFFF) == 25599);   /* never the 0xFFFF sentinel */
    assert(max17048_crate_x100(5) == 104);        /* 1.04 %/h */
    assert(max17048_crate_x100((uint16_t)-5) == -104);
    assert(max17048_crate_x100(0x7FFF) == 32767); /* clamped */
    assert(max17048_crate_x100(0x8000) == -32767);/* clamped, not 0x8000 */
    puts("MAX17048: SOC and charge-rate scaling, clamping away from sentinels passed");
}
''')
    compile_run(tmp, 'gauge', [tmp/'gauge.c'])
    bsp = 'Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini_radio.c'
    rf = '\n'.join(function(bsp, s) for s in [
        'static void rf_sw_wait_us(uint32_t us)', 'static void rf_sw_set_ctrl(bool high)',
        'void BSP_RADIO_SwitchPowerOn(void)', 'void BSP_RADIO_SwitchPowerOff(void)',
        'int32_t BSP_RADIO_ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config)'])
    (tmp/'rfsw.c').write_text(r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
typedef enum {RADIO_SWITCH_OFF, RADIO_SWITCH_RX, RADIO_SWITCH_RFO_LP, RADIO_SWITCH_RFO_HP} BSP_RADIO_Switch_TypeDef;
enum {GPIO_PIN_RESET, GPIO_PIN_SET};
#define BSP_ERROR_NONE 0
#define RF_SW_CTRL_GPIO_PORT 1
#define RF_SW_CTRL_PIN 13
#define RF_SW_VDD_GPIO_PORT 2
#define RF_SW_VDD_PIN 12
#define RF_SW_VDD_SETTLE_US 1000U
#define RF_SW_CTRL_SETTLE_US 250U
static uint32_t SystemCoreClock = 48000000U;
static int vdd, ctrl;
static uint64_t waited_since_vdd, waited_since_ctrl, time_us;
static void HAL_GPIO_WritePin(int port, int pin, int state)
{
    (void)pin;
    if (port == RF_SW_VDD_GPIO_PORT) {
        if (state && !vdd) waited_since_vdd = time_us;
        vdd = state;
    } else {
        /* A CTRL edge needs VDD up and settled. */
        if (state != ctrl) assert(vdd && time_us - waited_since_vdd >= 1000U);
        if (state != ctrl) waited_since_ctrl = time_us;
        ctrl = state;
    }
    assert(!(ctrl && !vdd));
}
static bool rf_sw_powered, rf_sw_ctrl_high, radio_asleep = true;
''' + rf.replace('for (volatile uint32_t n = us * (SystemCoreClock / 4000000U); n != 0U; --n) { }',
                 'time_us += us; (void)SystemCoreClock;') + r'''
static void radio_ready(void) { assert(vdd && time_us - waited_since_ctrl >= 250U); }
int main(void)
{
    /* Boot: LBM selects RX at init while the radio sleeps: stays unpowered. */
    BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RX); assert(!vdd && !ctrl);
    /* Uplink: hook powers up, TX selects CTRL HIGH. */
    BSP_RADIO_SwitchPowerOn(); BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RFO_HP);
    assert(vdd && ctrl); radio_ready();
    /* TX done: radio sleeps, planner then selects RX, hook stops. */
    BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_OFF); assert(!vdd && !ctrl);
    BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RX); assert(!vdd);
    BSP_RADIO_SwitchPowerOff(); assert(!vdd && !ctrl);
    /* RX1: hook powers up, RX keeps CTRL LOW. */
    time_us += 1000000U;
    BSP_RADIO_SwitchPowerOn(); BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RX);
    assert(vdd && !ctrl && time_us - waited_since_vdd >= 1000U);
    /* TX <-> RX while powered only toggles CTRL. */
    BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RFO_HP); radio_ready();
    BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RX); assert(vdd && !ctrl); radio_ready();
    /* A TX request without the hook still powers up first. */
    BSP_RADIO_SwitchPowerOff();
    BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RFO_LP); assert(vdd && ctrl); radio_ready();
    BSP_RADIO_SwitchPowerOff(); assert(!vdd && !ctrl);
    puts("RF switch: VDD before every CTRL edge, CTRL low before VDD off, no power-up after radio sleep passed");
}
''')
    compile_run(tmp, 'rfsw', [tmp/'rfsw.c'])
    adc = '\n'.join(function('Core/Src/adc_if.c', s) for s in [
        'static uint32_t adc_vdda_mv(uint32_t vref_raw, uint32_t vref_cal)',
        'static uint16_t adc_panel_mv(uint32_t pin_raw, uint32_t vdda_mv)',
        'static int16_t adc_temperature_x100(uint32_t ts_raw, uint32_t vdda_mv, uint32_t cal1, uint32_t cal2)'])
    (tmp/'adc.c').write_text(r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define VREFINT_CAL_VREF 3300UL
#define TEMPSENSOR_CAL_VREFANALOG 3300UL
#define TEMPSENSOR_CAL1_TEMP ((int32_t)30)
#define TEMPSENSOR_CAL2_TEMP ((int32_t)130)
#define SOLAR_DIVIDER_NUM 5U
#define SOLAR_DIVIDER_DEN 2U
#define ADC_FULL_SCALE 4095U
''' + adc + r'''
int main(void) {
    /* VREFINT read equals its 3.3 V calibration: VDDA 3300 mV; lower VDDA reads higher. */
    assert(adc_vdda_mv(1500, 1500) == 3300U && adc_vdda_mv(1650, 1500) == 3000U);
    /* Panel Voc max 6.91 V -> 2.764 V at PB2 -> 3430 counts at 3.3 V. */
    assert(adc_panel_mv(3430, 3300) == 6910U);
    assert(adc_panel_mv(0, 3300) == 0U && adc_panel_mv(4095, 3300) == 8250U);
    /* Temperature: TS_CAL1 at 30 degC, TS_CAL2 at 130 degC (both at 3.3 V). */
    assert(adc_temperature_x100(1000, 3300, 1000, 1300) == 3000);
    assert(adc_temperature_x100(1300, 3300, 1000, 1300) == 13000);
    assert(adc_temperature_x100(1075, 3300, 1000, 1300) == 5500);
    /* Same die temperature at VDDA 3.0 V reads proportionally higher counts. */
    assert(adc_temperature_x100(1100, 3000, 1000, 1300) == 3000);
    assert(adc_temperature_x100(925, 3300, 1000, 1300) == 500);
    puts("ADC: VREFINT-scaled VDDA, solar divider 2.5:1 and calibrated board temperature passed");
}
''')
    compile_run(tmp, 'adc', [tmp/'adc.c'])
    alarm = function('Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_supervisor/modem_supervisor_light.c',
                     'static uint32_t supervisor_check_user_alarm( void )')
    setter = function('Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem.c',
                      'smtc_modem_return_code_t smtc_modem_alarm_start_timer( uint32_t alarm_s )')
    source = r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
typedef struct { uint32_t Seconds; int16_t SubSeconds; } SysTime_t;
static uint64_t now_ms;
static uint32_t alarm;
static unsigned events;
static SysTime_t SysTimeGetMcuTime(void) {return (SysTime_t){now_ms/1000,now_ms%1000};}
static uint32_t modem_get_user_alarm(void) {return alarm;}
static void modem_set_user_alarm(uint32_t a) {alarm=a;}
static void increment_asynchronous_msgnumber(int a,int b,int c) {(void)a;(void)b;(void)c;events++;}
#define MODEM_MAX_ALARM_S 0x7FFFFFFF
#define MODEM_MAX_ALARM_VALUE_S 864000U
#define RETURN_BUSY_IF_TEST_MODE()
#define SMTC_MODEM_RC_INVALID 1
#define SMTC_MODEM_RC_OK 0
typedef int smtc_modem_return_code_t;
#define SMTC_MODEM_EVENT_ALARM 1
''' + alarm + '\n' + setter + r'''
int main(void) {
    const uint32_t intervals[]={30,180,3600,43200};
    for(unsigned i=0;i<4;i++) {
        now_ms=4294960000ULL;events=0;
        assert(smtc_modem_alarm_start_timer(intervals[i])==0);
        now_ms+=(uint64_t)intervals[i]*1000;
        supervisor_check_user_alarm();assert(events==1);
    }
    unsigned count=0;
    for(now_ms=0;now_ms<5ULL*365*86400000;now_ms+=180000) {
        assert(smtc_modem_alarm_start_timer(180)==0);events=0;
        now_ms+=179000;supervisor_check_user_alarm();assert(events==0);
        now_ms+=1000;supervisor_check_user_alarm();assert(events==1);
        now_ms-=180000;count++;
    }
    printf("Original supervisor function: %u alarms / five simulated years passed\n",count);
}
'''
    (tmp/'time.c').write_text(source)
    compile_run(tmp, 'time', [tmp/'time.c'])
    rtc = function('Core/Src/timer_if.c', 'uint32_t TIMER_IF_GetTime(uint16_t *mSeconds)')
    rtc_source = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define RTC_N_PREDIV_S 10
#define RTC_PREDIV_S 1023U
#define RTC_SR_SSRUF 1U
static struct {uint32_t SR;} rtc;
#define RTC (&rtc)
static uint32_t msb, low, primask;
static int wrap_during_read;
static uint32_t __get_PRIMASK(void) {return primask;}
static void __disable_irq(void) {primask=1;}
static void __set_PRIMASK(uint32_t p) {primask=p;}
static uint32_t TIMER_IF_BkUp_Read_MSBticks(void) {return msb;}
static uint32_t GetTimerTicks(void) {if(wrap_during_read){rtc.SR=1;low=0;wrap_during_read=0;}return low;}
static uint32_t TIMER_IF_Convert_Tick2ms(uint32_t t) {return ((uint64_t)t*1000)>>10;}
''' + rtc + r'''
int main(void) {
 uint16_t ms;
 low=UINT32_MAX;assert(TIMER_IF_GetTime(&ms)==4194303U && ms==999U && primask==0U);
 low=0;rtc.SR=1;assert(TIMER_IF_GetTime(&ms)==4194304U && ms==0U);
 msb=1;rtc.SR=0;assert(TIMER_IF_GetTime(&ms)==4194304U);
 msb=0;rtc.SR=0;low=UINT32_MAX;wrap_during_read=1;
 assert(TIMER_IF_GetTime(&ms)==4194304U && primask==0U);
 primask=1;assert(TIMER_IF_GetTime(&ms)==4194304U && primask==1U);
 puts("Actual RTC epoch reader: pre-wrap, pending IRQ, serviced IRQ, in-read wrap and nested critical section passed");
}
'''
    (tmp/'rtc.c').write_text(rtc_source)
    compile_run(tmp, 'rtc', [tmp/'rtc.c'])
    # Audit the entire port, not just the user-alarm call site.
    pattern = r'SysTimeToMs\s*\(\s*SysTimeGet\s*\(\s*\)\s*\)\s*/\s*1000'
    for path in (ROOT/'Middlewares').rglob('*'):
        if path.suffix in ('.c', '.h'):
            assert not re.search(pattern,path.read_text()), path
    app=(ROOT/'LoRaWAN/App/lora_app.c').read_text()
    for name in ['OnScd41TimerEvent','OnScd41RestartTimerEvent','OnSps30TimerEvent','OnSps30CleanupTimerEvent','OnSolarTimerEvent']:
        body=function('LoRaWAN/App/lora_app.c', f'static void {name}(void *context)')
        assert not re.search(r'EnvSensors_|SPS30_|INA226_|SampleSolar|HAL_Delay|HAL_I2C', body), name
    print('Port audit: no truncated seconds clocks; sensor timer IRQs contain no I/O')
print('All host tests passed. Hardware fault injection and power measurements remain required.')
