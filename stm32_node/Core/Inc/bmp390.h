/**
 ******************************************************************************
 * @file    bmp390.h
 * @brief   BMP390 barometric pressure sensor driver — public API
 *
 * Measurement mode: Forced (single-shot → sensor returns to sleep automatically).
 * Weather station settings (BMP390 datasheet Table 10):
 *   osrs_p = ×1 (Ultra Low Power), osrs_t = ×1, IIR = off
 *   → 4 µA average, ODR 1/60 Hz, ~55 cm pressure noise.
 ******************************************************************************
 */

#ifndef BMP390_H
#define BMP390_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Return codes                                                               */
/* -------------------------------------------------------------------------- */
#define BMP390_OK        ( 0)
#define BMP390_ERR_I2C   (-1)   /* HAL I2C error                             */
#define BMP390_ERR_ID    (-2)   /* CHIP_ID mismatch (not 0x60)               */
#define BMP390_ERR_PARAM (-3)   /* Invalid function parameter                */
#define BMP390_ERR_INIT  (-4)   /* Sensor is not initialised                 */

/* -------------------------------------------------------------------------- */
/* Sensor output                                                              */
/* -------------------------------------------------------------------------- */
typedef struct
{
    float pressure_hPa;  /* Compensated pressure in hPa                       */
    float temperature;   /* Compensated temperature in °C (from BMP390 sensor) */
} BMP390_Data_t;

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Initialise the BMP390.
 *         Performs soft-reset, verifies CHIP_ID (0x60), reads 21-byte NVM
 *         calibration block and converts coefficients.  Configures OSR and
 *         IIR registers for the weather-station profile.  The sensor is left
 *         in Sleep mode (idle, ~1.4 µA) after this call.
 * @retval BMP390_OK on success, BMP390_ERR_I2C or BMP390_ERR_ID on failure.
 */
int32_t BMP390_Init(void);

/**
 * @brief  Trigger a single Forced-mode measurement, read and compensate the
 *         result, then return.  The sensor automatically returns to Sleep mode
 *         after the measurement — no explicit sleep command needed.
 * @param  data  Pointer to output structure.
 * @retval BMP390_OK on success, BMP390_ERR_I2C on failure.
 */
int32_t BMP390_Read(BMP390_Data_t *data);

#endif /* BMP390_H */
