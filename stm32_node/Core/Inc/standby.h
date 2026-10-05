#ifndef STANDBY_H
#define STANDBY_H
#include <stdbool.h>
/* Deep-discharge Standby below RECOVERY (TD_2_0_18).
 * In Standby only VCC (U3), the RTC with the LSE and the MAX17048 (in
 * hibernate) keep running; the GPIOs float, so the 1 MOhm pull-downs switch
 * +5 V and 3V3SWITCHABLE off. The RTC alarm wakes the MCU every
 * STANDBY_WAKE_MIN; waking is a reset. */
#define STANDBY_WAKE_MIN  60U

/* The IWDG must stop in Standby: option byte IWDG_STDBY = 0, set once with
 * STM32CubeProgrammer. Without it the watchdog would reset the MCU within
 * 32 s and Standby is refused (the station stays in RECOVERY). */
bool Standby_IwdgReady(void);
/* Very early in main(), before the rails come up: after a Standby wake-up,
 * read only the MAX17048 and go back to Standby below POWER_RESTART_MV.
 * Returns for a normal boot. */
void Standby_CheckWake(void);
/* Shuts down what still draws current, records restart reason 8, arms the
 * RTC wake-up and enters Standby. Does not return. */
void Standby_Enter(void) __attribute__((noreturn));
#endif
