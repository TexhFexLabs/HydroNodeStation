#ifndef __BQ25185_H__
#define __BQ25185_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* BQ25185 battery charger CE (charge enable) pin, active low, on PA2
 * (U8.4 -> U10.9). The datasheet documents no internal pull-down on CE; the
 * LOW level comes from R7 (10 kOhm to GND). Leaving the MCU pin floating
 * (analog mode) therefore keeps charging enabled at zero GPIO cost. Driving
 * CE high disables charging and costs about 330 uA through R7, but only for
 * the pulse; the high -> low transition restarts the charger's internal
 * safety timer (expires after ~6 h of charging). */
#define BQ25185_CE_PORT             CHG_CE_GPIO_Port
#define BQ25185_CE_PIN              CHG_CE_Pin

/* CE high time used when resetting the safety timer */
#define BQ25185_CE_RESET_PULSE_MS   500U

void BQ25185_ChargeDisable(void);
void BQ25185_ChargeEnable(void);

#ifdef __cplusplus
}
#endif

#endif /* __BQ25185_H__ */
