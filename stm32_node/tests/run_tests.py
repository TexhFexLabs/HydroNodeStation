#!/usr/bin/env python3
"""Host regression tests. No STM32 hardware or third-party Python packages needed."""
from pathlib import Path
import re
import subprocess
import tempfile
import test_sps30
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
    compile_run(tmp, 'power', [ROOT/'tests/test_power.c', ROOT/'Core/Src/power_policy.c'])
    compile_run(tmp, 'nvm', [ROOT/'tests/test_nvm.c', ROOT/'Core/Src/nvm_store.c'])
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
    for name in ['OnScd41TimerEvent','OnScd41RestartTimerEvent','OnSps30TimerEvent','OnSps30CleanupTimerEvent']:
        body=function('LoRaWAN/App/lora_app.c', f'static void {name}(void *context)')
        assert not re.search(r'EnvSensors_|SPS30_|HAL_Delay|HAL_I2C', body), name
    print('Port audit: no truncated seconds clocks; sensor timer IRQs contain no I/O')
print('All host tests passed. Hardware fault injection and power measurements remain required.')
