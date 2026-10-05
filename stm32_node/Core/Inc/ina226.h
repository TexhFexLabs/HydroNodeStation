/**
 ******************************************************************************
 * @file    ina226.h
 * @brief   INA226 current/power monitor at the solar input - public API
 *
 * I2C address: 0x40 (7-bit), VS on VCC. Shunt U16 = 68 mOhm between the
 * panel (IN+, VBUS) and D1/charger (IN-). ALERT is not connected.
 ******************************************************************************
 */
#ifndef INA226_H
#define INA226_H
#include <stdint.h>

#define INA226_OK           ( 0)
#define INA226_ERR_I2C      (-1)
#define INA226_ERR_PARAM    (-2)
#define INA226_ERR_INIT     (-3)
#define INA226_ERR_TIMEOUT  (-4)

#define INA226_I2C_ADDR_7B  (0x40U)

typedef struct
{
    uint16_t bus_mv;        /* Panel voltage in mV */
    uint16_t current_01ma;  /* Panel current in 0.1 mA, negative values clamped to 0 */
} INA226_Data_t;

/* Checks the IDs, writes the calibration and leaves the device powered down. */
int32_t INA226_Init(void);
/* One triggered shunt+bus conversion (about 36 ms), then power-down again.
 * Thread mode only: blocks while the conversion runs. */
int32_t INA226_Measure(INA226_Data_t *data);
/* Power-down (mode 000), about 2 uA. */
int32_t INA226_Shutdown(void);
/* HAL_I2C error bits of the last failed transfer, 0 otherwise. */
uint32_t INA226_LastI2cError(void);
#endif /* INA226_H */
