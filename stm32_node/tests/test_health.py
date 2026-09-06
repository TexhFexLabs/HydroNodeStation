from pathlib import Path
import subprocess

def run(root,tmp):
    stub=tmp/'health-stubs';stub.mkdir()
    (stub/'platform.h').write_text('''#include <stdint.h>
struct RCC_Model {uint32_t CSR;};
struct IWDG_Model {uint32_t KR,PR,RLR,SR;};
extern struct RCC_Model rcc;
extern struct IWDG_Model iwdg;
#define RCC (&rcc)
#define IWDG (&iwdg)
#define __HAL_RCC_CLEAR_RESET_FLAGS() (rcc.CSR=0)
void __disable_irq(void);
void NVIC_SystemReset(void);
''')
    (stub/'rtc.h').write_text('''#include <stdint.h>
extern int hrtc;
#define RTC_BKP_DR3 3
#define RTC_BKP_DR4 4
#define RTC_BKP_DR5 5
#define RTC_BKP_DR6 6
uint32_t HAL_RTCEx_BKUPRead(int *,unsigned);
void HAL_RTCEx_BKUPWrite(int *,unsigned,uint32_t);
''')
    (stub/'stm32_systime.h').write_text('''#include <stdint.h>
typedef struct {uint32_t Seconds;} SysTime_t;
SysTime_t SysTimeGetMcuTime(void);
''')
    (tmp/'health-model.c').write_text('''#include "runtime_health.h"
#include "platform.h"
#include "rtc.h"
#include "stm32_systime.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
struct RCC_Model rcc={0x80000000U};struct IWDG_Model iwdg;
int hrtc;static uint32_t backup[7],now;static jmp_buf reset;
void __disable_irq(void) {}
void NVIC_SystemReset(void) {longjmp(reset,1);}
uint32_t HAL_RTCEx_BKUPRead(int *h,unsigned r) {(void)h;return backup[r];}
void HAL_RTCEx_BKUPWrite(int *h,unsigned r,uint32_t v) {(void)h;backup[r]=v;}
SysTime_t SysTimeGetMcuTime(void) {return (SysTime_t){now};}
int main(void)
{
 Runtime_EarlyInit();Runtime_Init();assert(Runtime_BootCount()==1);
 assert(Runtime_ResetFlags()==0x80000000U && iwdg.KR==0xAAAAU);
 Runtime_ExpectProgress(180);now=180;Runtime_Process();
 if(!setjmp(reset)) {now=181;Runtime_Process();assert(0);}
 assert(backup[6]==RUNTIME_FAULT_PROGRESS);
 Runtime_Init();assert(Runtime_BootCount()==2 && Runtime_LastFault()==RUNTIME_FAULT_PROGRESS);
 now=UINT32_MAX-20;Runtime_ExpectProgress(30);now=9;Runtime_Process();
 Runtime_ExpectProgress(0);now=10000000;Runtime_Process();
 if(!setjmp(reset)) {Runtime_Fault(RUNTIME_FAULT_MODEM);assert(0);}
 assert(backup[6]==RUNTIME_FAULT_MODEM);
 puts("Actual health module: progress deadline, reset diagnostics, seconds wrap and deliberate dormancy passed");
}
''')
    exe=tmp/'health'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(stub),'-I'+str(root/'Core/Inc'),
                    str(tmp/'health-model.c'),str(root/'Core/Src/runtime_health.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
