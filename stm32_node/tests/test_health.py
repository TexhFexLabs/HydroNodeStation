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
#define LED_DIAG_GPIO_Port 1
#define LED_DIAG_Pin 8U
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
void __disable_irq(void);
void NVIC_SystemReset(void);
void HAL_GPIO_WritePin(int,unsigned,int);
typedef struct {unsigned Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
void HAL_GPIO_Init(int,GPIO_InitTypeDef *);
void HAL_PWR_EnableBkUpAccess(void);
#define __HAL_RCC_RTCAPB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
''')
    (stub/'rtc.h').write_text('''#include <stdint.h>
extern int hrtc;
#define RTC_BKP_DR3 3
#define RTC_BKP_DR4 4
#define RTC_BKP_DR5 5
#define RTC_BKP_DR6 6
#define RTC_BKP_DR7 7
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
int hrtc;static uint32_t backup[8],now;static jmp_buf reset;
static unsigned blinks,led=GPIO_PIN_SET;
void HAL_GPIO_WritePin(int port,unsigned pin,int state)
{
 assert(port==LED_DIAG_GPIO_Port && pin==LED_DIAG_Pin);
 if(state==GPIO_PIN_RESET && led==GPIO_PIN_SET) blinks++;
 led=(unsigned)state;
}
void __disable_irq(void) {}
static unsigned gpio_inits;
void HAL_GPIO_Init(int port,GPIO_InitTypeDef *g) {assert(port==LED_DIAG_GPIO_Port && g->Pin==LED_DIAG_Pin);gpio_inits++;}
void HAL_PWR_EnableBkUpAccess(void) {}
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
 assert(backup[6]==RUNTIME_FAULT_PROGRESS && backup[7]==0);
 assert(blinks==RUNTIME_FAULT_PROGRESS && led==GPIO_PIN_SET);blinks=0;
 Runtime_Init();assert(Runtime_BootCount()==2 && Runtime_LastFault()==RUNTIME_FAULT_PROGRESS);
 now=UINT32_MAX-20;Runtime_ExpectProgress(30);now=9;Runtime_Process();
 Runtime_ExpectProgress(0);now=10000000;Runtime_Process();
 /* Non-fatal anomalies count and saturate; they never reset. */
 for(unsigned i=0;i<70000;i++) Runtime_NotePanic();
 assert(Runtime_PanicCount()==65535U);
 for(unsigned i=0;i<300;i++) Runtime_NoteNvmError(3);
 assert(Runtime_NvmErrorCount()==255U && Runtime_LastNvmError()==3);
 Runtime_FaultDetail(7);
 if(!setjmp(reset)) {Runtime_Fault(RUNTIME_FAULT_MODEM);assert(0);}
 assert(backup[6]==RUNTIME_FAULT_MODEM && backup[7]==7);
 /* Reason group then detail group, both blinked out. */
 assert(blinks==RUNTIME_FAULT_MODEM+7 && led==GPIO_PIN_SET);
 Runtime_Init();assert(Runtime_LastFault()==RUNTIME_FAULT_MODEM && Runtime_LastFaultDetail()==7);
 /* Clock-setup fault before Runtime_Init: reason stored, LED set up and the
  * code repeated until the hold time is over, then a reset. */
 blinks=0;backup[7]=9;
 if(!setjmp(reset)) {Runtime_EarlyFault(RUNTIME_FAULT_LSE,0);assert(0);}
 assert(backup[6]==RUNTIME_FAULT_LSE && backup[7]==0 && gpio_inits==1);
 assert(blinks==RUNTIME_FAULT_LSE && led==GPIO_PIN_SET);
 blinks=0;
 if(!setjmp(reset)) {Runtime_EarlyFault(RUNTIME_FAULT_LSE,9);assert(0);}
 assert(blinks==3*RUNTIME_FAULT_LSE);  /* 9 s = 36 units, 18 units per group */
 Runtime_Init();assert(Runtime_LastFault()==RUNTIME_FAULT_LSE);
 puts("Actual health module: progress deadline, reset diagnostics, fault blink code, seconds wrap, deliberate dormancy and early LSE fault passed");
}
''')
    exe=tmp/'health'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(stub),'-I'+str(root/'Core/Inc'),
                    str(tmp/'health-model.c'),str(root/'Core/Src/runtime_health.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
