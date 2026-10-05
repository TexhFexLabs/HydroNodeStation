/**
 ******************************************************************************
 * @file    max17048.h
 * @brief   MAX17048 fuel gauge driver - public API
 *
 * I2C address: 0x36 (7-bit)
 * Exposed measurements: cell voltage, state of charge and charge rate.
 * The gauge runs permanently in hibernate (HIBRT = 0xFFFF); ALRT is unused.
 ******************************************************************************
 */

#ifndef MAX17048_H
#define MAX17048_H

#include <stdint.h>

#define MAX17048_OK           ( 0)
#define MAX17048_ERR_I2C      (-1)
#define MAX17048_ERR_PARAM    (-2)
#define MAX17048_ERR_INIT     (-3)

typedef struct
{
    uint16_t voltage_mv;  /* Cell voltage in millivolts */
} MAX17048_Data_t;

typedef struct
{
    uint16_t soc_x100;    /* State of charge in 0.01 %, MAX17048_SOC_INVALID if unknown */
    int16_t  crate_x100;  /* Charge rate in 0.01 %/h, MAX17048_CRATE_INVALID if unknown */
} MAX17048_Gauge_t;

#define MAX17048_SOC_INVALID   (0xFFFFU)
#define MAX17048_CRATE_INVALID (INT16_MIN)

int32_t MAX17048_Init(void);
/* SOC and charge rate for the payload block; also clears a pending alert. */
int32_t MAX17048_ReadGauge(MAX17048_Gauge_t *gauge);
int32_t MAX17048_Read(MAX17048_Data_t *data);

#endif /* MAX17048_H */
