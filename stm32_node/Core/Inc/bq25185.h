#ifndef __BQ25185_H__
#define __BQ25185_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* BQ25185 battery charger CE (charge enable) pin, active low.
 * The charger has an internal pull-down on CE, so leaving the MCU pin
 * floating (analog mode) keeps charging enabled at zero GPIO cost.
 * Driving CE high disables charging; the high -> low transition restarts
 * the charger's internal safety timer (expires after ~6 h of charging). */
#define BQ25185_CE_PORT             GPIOA
#define BQ25185_CE_PIN              GPIO_PIN_7

/* CE high time used when resetting the safety timer */
#define BQ25185_CE_RESET_PULSE_MS   500U

void BQ25185_ChargeDisable(void);
void BQ25185_ChargeEnable(void);

#ifdef __cplusplus
}
#endif

#endif /* __BQ25185_H__ */
