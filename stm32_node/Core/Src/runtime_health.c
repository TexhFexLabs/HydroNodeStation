#include "runtime_health.h"
#include "platform.h"
#include "rtc.h"
#include "stm32_systime.h"

#define HEALTH_MAGIC 0x484E4431U
static uint32_t reset_flags, boot_count, last_fault;
static uint32_t progress_at, progress_timeout;
static uint8_t ready;

void Runtime_EarlyInit(void)
{
    reset_flags = RCC->CSR;
    /* IWDG uses LSI independently of the system/RTC clocks. About 32 s at
     * nominal LSI; sleep is capped at 8 s to cover oscillator tolerances. */
    IWDG->KR = 0xCCCCU;
    IWDG->KR = 0x5555U;
    IWDG->PR = 6U; /* /256 */
    IWDG->RLR = 4095U;
    uint32_t guard = 1000000U;
    while (IWDG->SR != 0U && --guard != 0U) { }
    IWDG->KR = 0xAAAAU;
}

void Runtime_Init(void)
{
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR3) == HEALTH_MAGIC)
    {
        boot_count = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR4);
        last_fault = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR6);
    }
    boot_count++;
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR3, HEALTH_MAGIC);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR4, boot_count);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR5, reset_flags);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR6, 0U);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    ready = 1U;
    Runtime_ExpectProgress(1800U);
    Runtime_Process();
}

void Runtime_ExpectProgress(uint32_t timeout_s)
{
    progress_at = SysTimeGetMcuTime().Seconds;
    progress_timeout = timeout_s;
}

void Runtime_Process(void)
{
    if (ready && progress_timeout != 0U &&
        (uint32_t)(SysTimeGetMcuTime().Seconds - progress_at) > progress_timeout)
    {
        Runtime_Fault(RUNTIME_FAULT_PROGRESS);
    }
    /* Feed only from thread mode, after application processing. */
    IWDG->KR = 0xAAAAU;
}

void Runtime_Fault(uint32_t reason)
{
    __disable_irq();
    if (ready) { HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR6, reason); }
    NVIC_SystemReset();
    for (;;) { }
}
uint32_t Runtime_BootCount(void) { return boot_count; }
uint32_t Runtime_ResetFlags(void) { return reset_flags; }
uint32_t Runtime_LastFault(void) { return last_fault; }
