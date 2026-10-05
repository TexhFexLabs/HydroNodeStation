#include "led.h"
#include "main.h"
#include "stm32_timer.h"
#include <stdbool.h>

typedef struct
{
    UTIL_TIMER_Object_t timer;
    bool created, lit;
    uint8_t left;
    uint16_t on_ms, off_ms;
} led_state_t;

static led_state_t leds[2];
static GPIO_TypeDef *const port[2] = { LED1_GPIO_Port, LED_DIAG_GPIO_Port };
static const uint16_t pin[2] = { LED1_Pin, LED_DIAG_Pin };

static void led_drive(led_t led, bool on)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin[led];
    if (on)
    {
        HAL_GPIO_WritePin(port[led], pin[led], GPIO_PIN_RESET);
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
    }
    else
    {
        gpio.Mode = GPIO_MODE_ANALOG;
    }
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port[led], &gpio);
    leds[led].lit = on;
}

static void led_step(void *context)
{
    led_t led = (led_t)(uintptr_t)context;
    led_state_t *s = &leds[led];
    if (s->lit)
    {
        led_drive(led, false);
        if (s->left == 0U) return;
        UTIL_TIMER_SetPeriod(&s->timer, s->off_ms);
    }
    else
    {
        led_drive(led, true);
        s->left--;
        UTIL_TIMER_SetPeriod(&s->timer, s->on_ms);
    }
    UTIL_TIMER_Start(&s->timer);
}

void Led_Flash(led_t led, uint8_t count, uint16_t on_ms, uint16_t off_ms)
{
    led_state_t *s = &leds[led];
    if (!s->created)
    {
        UTIL_TIMER_Create(&s->timer, on_ms, UTIL_TIMER_ONESHOT, led_step, (void *)(uintptr_t)led);
        s->created = true;
    }
    UTIL_TIMER_Stop(&s->timer);
    if (count == 0U) { led_drive(led, false); return; }
    s->on_ms = on_ms;
    s->off_ms = off_ms;
    s->left = (uint8_t)(count - 1U);
    led_drive(led, true);
    UTIL_TIMER_SetPeriod(&s->timer, on_ms);
    UTIL_TIMER_Start(&s->timer);
}

void Led_Off(led_t led)
{
    if (leds[led].created) UTIL_TIMER_Stop(&leds[led].timer);
    led_drive(led, false);
}
