#include "pulse_counter.h"
#include "sys_conf.h"

bool PulseCounter_Edge(pulse_counter_t *pc, uint8_t ch, uint32_t now_ms, uint32_t lockout_ms)
{
    /* Contact bounce: ignore edges inside the lockout after a counted one. */
    if (pc->seen[ch] && (uint32_t)(now_ms - pc->last_ms[ch]) < lockout_ms) { return false; }
    pc->seen[ch] = true;
    pc->last_ms[ch] = now_ms;
    pc->total[ch]++;
    return true;
}

uint16_t PulseCounter_Since(const pulse_counter_t *pc, uint8_t ch)
{
    uint32_t n = pc->total[ch] - pc->committed[ch];
    return (n > PULSE_MAX) ? (uint16_t)PULSE_MAX : (uint16_t)n;
}

void PulseCounter_Advance(pulse_counter_t *pc, uint8_t ch, uint16_t reported)
{
    /* A saturated report drops the excess instead of carrying it over. */
    pc->committed[ch] = (reported == PULSE_MAX) ? pc->total[ch] : pc->committed[ch] + reported;
}

#if PULSE_COUNTERS_ENABLED
#include "main.h"
#include "stm32_timer.h"

static pulse_counter_t counters;

void PulseCounter_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = CNT1_Pin | CNT2_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
    /* Below the radio and RTC interrupts: a count may wait, a RX window not. */
    HAL_NVIC_SetPriority(EXTI4_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t now = UTIL_TIMER_GetCurrentTime();
    if (GPIO_Pin == CNT1_Pin) { (void)PulseCounter_Edge(&counters, 0U, now, PULSE_CNT1_LOCKOUT_MS); }
    else if (GPIO_Pin == CNT2_Pin) { (void)PulseCounter_Edge(&counters, 1U, now, PULSE_CNT2_LOCKOUT_MS); }
}

void PulseCounter_Read(uint16_t *cnt1, uint16_t *cnt2)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *cnt1 = PulseCounter_Since(&counters, 0U);
    *cnt2 = PulseCounter_Since(&counters, 1U);
    __set_PRIMASK(primask);
}

void PulseCounter_Commit(uint16_t cnt1, uint16_t cnt2)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    PulseCounter_Advance(&counters, 0U, cnt1);
    PulseCounter_Advance(&counters, 1U, cnt2);
    __set_PRIMASK(primask);
}
#else
void PulseCounter_Init(void) { }

void PulseCounter_Read(uint16_t *cnt1, uint16_t *cnt2)
{
    *cnt1 = PULSE_MISSING;
    *cnt2 = PULSE_MISSING;
}

void PulseCounter_Commit(uint16_t cnt1, uint16_t cnt2)
{
    (void)cnt1;
    (void)cnt2;
}
#endif
