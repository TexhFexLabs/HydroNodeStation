/**
 ******************************************************************************
 * @file    max17048.h
 * @brief   MAX17048 fuel gauge driver - public API
 *
 * I2C address: 0x36 (7-bit)
 * Exposed measurement: battery cell voltage.
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
    float voltage_v;      /* Cell voltage in volts */
    uint16_t voltage_mv;  /* Cell voltage in millivolts */
} MAX17048_Data_t;

int32_t MAX17048_Init(void);
int32_t MAX17048_Read(MAX17048_Data_t *data);

#endif /* MAX17048_H */
