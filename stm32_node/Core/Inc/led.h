#ifndef LED_H
#define LED_H
#include <stdint.h>
/* LED1 (PB5 via DIP 1) and LED_DIAG (PB3 via DIP 2): VCC -> 1 kOhm -> LED
 * -> DIP -> pin, active low, lit only with the DIP closed. Between patterns
 * the pins are analog, so nothing leaks through a closed DIP. */
typedef enum { LED_1 = 0, LED_DIAG = 1 } led_t;

/* Non-blocking: count flashes of on_ms with off_ms gaps, driven by a UTIL
 * timer (the callback only writes the GPIO). Replaces a running pattern. */
void Led_Flash(led_t led, uint8_t count, uint16_t on_ms, uint16_t off_ms);
void Led_Off(led_t led);
#endif
