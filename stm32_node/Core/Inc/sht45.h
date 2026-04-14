/**
 ******************************************************************************
 * @file    sht45.h
 * @brief   SHT45 temperature and humidity sensor driver
 *
 * Uses hardware I2C2 (hi2c2, configured by MX_I2C2_Init).
 * Hardware connections:
 *   PA15  ->  SDA  (I2C2, AF4, open-drain, external 4.7k pull-up to VDD)
 *   PB15  ->  SCL  (I2C2, AF4, open-drain, external 4.7k pull-up to VDD)
 *
 * Datasheet: Sensirion SHT4x Datasheet, Document Version 6, Feb 2023
 *   I2C address: 0x44 (ADDR connected to VSS)
 ******************************************************************************
 */

#ifndef SHT45_H
#define SHT45_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Return codes                                                               */
/* -------------------------------------------------------------------------- */
#define SHT45_OK       ( 0)
#define SHT45_ERR_CRC  (-1)
#define SHT45_ERR_I2C  (-2)

/* -------------------------------------------------------------------------- */
/* Sensor data                                                                */
/* -------------------------------------------------------------------------- */
typedef struct
{
    float temperature;   /* Celsius */
    float humidity;      /* %RH, range 0..100 */
} SHT45_Data_t;

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Mark the driver as initialised.
 *         Call MX_I2C2_Init() before this function.
 */
void    SHT45_Init(void);

/**
 * @brief  Trigger a single-shot high-precision measurement and read results.
 *
 * Issues command 0xFD (high repeatability, no clock stretching, no heater).
 * Measurement time: typ 8.2 ms, max 9.4 ms — driver waits 10 ms.
 *
 * @param  data  Filled with temperature (°C) and humidity (%RH) on success.
 * @retval SHT45_OK       Success
 * @retval SHT45_ERR_I2C  No ACK (sensor absent / wiring error)
 * @retval SHT45_ERR_CRC  Checksum mismatch
 */
int32_t SHT45_Read(SHT45_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* SHT45_H */
