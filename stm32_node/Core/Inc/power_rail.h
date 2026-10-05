#ifndef __POWER_RAIL_H__
#define __POWER_RAIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/* Switchable supply rails on PCB 1.1, each a TPS63900 with a 1 MOhm
 * pull-down on EN: without firmware, and in Standby where the GPIOs float,
 * both rails are off.
 * - 3V3SWITCHABLE (U1, EN3V3SW = PA8): nothing of this station hangs on it,
 *   so it stays off. Later sensors on that rail only need to activate it.
 * - +5 V (U4, EN5V = PA9): SPS30 supply, on from boot and kept on; the
 *   SPS30 sleeps over I2C between measurements. */
typedef enum { POWER_RAIL_3V3SW, POWER_RAIL_5V } power_rail_t;

/* TPS63900 td(EN) 1.5 ms plus soft start, with margin (POWER_ANALYSIS F6). */
#define POWER_RAIL_SETTLE_MS     5U
/* The SPS30 datasheet gives no interface-ready time after power-on;
 * Sensirion's own driver waits 100 ms after a reset. */
#define POWER_RAIL_SPS30_BOOT_MS 100U

/* Boot state: 3V3SWITCHABLE off, +5 V on and settled for the SPS30. */
void PowerRail_Init(void);
void PowerRail_3V3Sw_Activate(void);
void PowerRail_3V3Sw_Deactivate(void);
void PowerRail_5V_Activate(void);
void PowerRail_5V_Deactivate(void);
bool PowerRail_IsOn(power_rail_t rail);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_RAIL_H__ */
