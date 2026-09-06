#include "runtime_health.h"
#include "platform.h"
#include "rtc.h"
#include "stm32_systime.h"

#define HEALTH_MAGIC 0x484E4431U
static uint32_t reset_flags, boot_count, last_fault, last_detail, fault_detail;
static uint16_t panic_count;
static uint8_t nvm_error_count, last_nvm_error;
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
        last_detail = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR7);
    }
    boot_count++;
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR3, HEALTH_MAGIC);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR4, boot_count);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR5, reset_flags);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR6, 0U);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR7, 0U);
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

/* Roughly 250 ms of busy wait at the 48 MHz system clock. Interrupts and the
 * time base are already down when a fault is reported, so no timer is usable. */
#define FAULT_BLINK_LOOPS 2400000U

static void fault_wait(uint32_t units)
{
    while (units-- != 0U) { for (volatile uint32_t d = 0U; d < FAULT_BLINK_LOOPS; ++d) { } }
}

/* Fault codes go to the diagnostic LED, never to LED1: the startup blink on
 * LED1 must stay distinguishable from a fault. Active low. */
static void fault_blink(uint32_t count)
{
    for (uint32_t i = 0U; i < count; ++i)
    {
        HAL_GPIO_WritePin(LED_DIAG_GPIO_Port, LED_DIAG_Pin, GPIO_PIN_RESET);
        fault_wait(1U);
        HAL_GPIO_WritePin(LED_DIAG_GPIO_Port, LED_DIAG_Pin, GPIO_PIN_SET);
        fault_wait(1U);
    }
}

void Runtime_FaultDetail(uint32_t detail) { fault_detail = detail; }

void Runtime_NotePanic(void)
{
    if (panic_count != UINT16_MAX) { panic_count++; }
}

void Runtime_NoteNvmError(uint32_t detail)
{
    if (nvm_error_count != UINT8_MAX) { nvm_error_count++; }
    last_nvm_error = (uint8_t)detail;
}

uint16_t Runtime_PanicCount(void) { return panic_count; }
uint8_t Runtime_NvmErrorCount(void) { return nvm_error_count; }
uint8_t Runtime_LastNvmError(void) { return last_nvm_error; }

void Runtime_Fault(uint32_t reason)
{
    __disable_irq();
    if (ready)
    {
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR6, reason);
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR7, fault_detail);
    }
    /* Blink the reason on LED1 before resetting: an unattended reset is
     * otherwise undiagnosable without the trace UART or a debugger.
     * A second group after
     * a longer gap carries the detail, so "NVM failed" says which step failed. */
    fault_blink(reason);
    if (fault_detail != 0U)
    {
        fault_wait(6U);
        fault_blink(fault_detail);
    }
    NVIC_SystemReset();
    for (;;) { }
}
uint32_t Runtime_BootCount(void) { return boot_count; }
uint32_t Runtime_ResetFlags(void) { return reset_flags; }
uint32_t Runtime_LastFault(void) { return last_fault; }
uint32_t Runtime_LastFaultDetail(void) { return last_detail; }
