/**
 ******************************************************************************
 * @file    sht45.c
 * @brief   SHT45 temperature and humidity sensor driver
 *
 * Protocol (Sensirion SHT4x Datasheet, Table 4 / Section 4.5):
 *   Command 0xFD: single-shot measurement, high repeatability.
 *   1. Write command byte 0xFD to address 0x44.
 *   2. Wait for measurement (max 9.4 ms).
 *   3. Read 6 bytes:
 *        [0] T_MSB  [1] T_LSB  [2] CRC_T
 *        [3] RH_MSB [4] RH_LSB [5] CRC_RH
 *   Conversion (Datasheet Eq. 1 & 2):
 *        T  [°C]  = -45 + 175 * S_T  / 65535
 *        RH [%]   = -6  + 125 * S_RH / 65535   (clamped to [0, 100])
 *   CRC: polynomial 0x31, initial value 0xFF, MSB-first, no reflection/XOR.
 *
 * The sensor has no sleep command — it idles at low power between
 * single-shot measurements. Deep sleep is handled by the STM32 (STOP2).
 ******************************************************************************
 */

#include "sht45.h"
#include "i2c.h"
#include "stm32wlxx_hal.h"

/* -------------------------------------------------------------------------- */
/* Private constants                                                          */
/* -------------------------------------------------------------------------- */
#define SHT45_I2C_ADDR       (0x44U)   /* 7-bit address                       */
#define SHT45_CMD_MEAS_HIGH  (0xFDU)   /* High repeatability (Table 4)        */
#define SHT45_MEASURE_MS     (10U)     /* >= 9.4 ms max per datasheet         */
#define SHT45_I2C_TIMEOUT_MS (50U)

#define SHT45_CRC_POLY       (0x31U)
#define SHT45_CRC_INIT       (0xFFU)

#ifndef SHT45_ERR_PARAM
#define SHT45_ERR_PARAM      (-3)
#endif

/* -------------------------------------------------------------------------- */
/* Private helpers                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief CRC-8 over two data bytes.
 * Polynomial: x^8 + x^5 + x^4 + 1 (0x31), init 0xFF, MSB-first.
 */
static uint8_t sht45_crc8(uint8_t byte1, uint8_t byte2)
{
    uint8_t crc = SHT45_CRC_INIT;
    uint8_t data[2] = {byte1, byte2};

    for (uint8_t b = 0U; b < 2U; b++)
    {
        crc ^= data[b];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if (crc & 0x80U)
            {
                crc = (uint8_t)((crc << 1U) ^ SHT45_CRC_POLY);
            }
            else
            {
                crc <<= 1U;
            }
        }
    }
    return crc;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

int32_t SHT45_Init(void)
{
    HAL_StatusTypeDef status;

    /* I2C2 is initialised by MX_I2C2_Init() before this call.
       SHT45 needs no init command, so we just probe address 0x44. */
    status = HAL_I2C_IsDeviceReady(&hi2c2,
                                   (uint16_t)(SHT45_I2C_ADDR << 1U),
                                   2U,
                                   SHT45_I2C_TIMEOUT_MS);

    return (status == HAL_OK) ? SHT45_OK : SHT45_ERR_I2C;
}

int32_t SHT45_Read(SHT45_Data_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t cmd = SHT45_CMD_MEAS_HIGH;
    uint8_t buf[6];
    float rh;

    if (data == NULL)
    {
        return SHT45_ERR_PARAM;
    }

    /* 1. Trigger single-shot measurement ------------------------------------ */
    status = HAL_I2C_Master_Transmit(&hi2c2,
                                     (uint16_t)(SHT45_I2C_ADDR << 1U),
                                     &cmd, 1U,
                                     SHT45_I2C_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return SHT45_ERR_I2C;
    }

    /* 2. Wait for measurement to complete (max 9.4 ms) ---------------------- */
    HAL_Delay(SHT45_MEASURE_MS);

    /* 3. Read 6 result bytes ------------------------------------------------ */
    status = HAL_I2C_Master_Receive(&hi2c2,
                                    (uint16_t)(SHT45_I2C_ADDR << 1U),
                                    buf, 6U,
                                    SHT45_I2C_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return SHT45_ERR_I2C;
    }

    /* 4. Verify CRC for temperature and humidity words ---------------------- */
    if (sht45_crc8(buf[0], buf[1]) != buf[2])
    {
        return SHT45_ERR_CRC;
    }
    if (sht45_crc8(buf[3], buf[4]) != buf[5])
    {
        return SHT45_ERR_CRC;
    }

    /* 5. Convert raw values (Datasheet Eq. 1 & 2) -------------------------- */
    uint16_t raw_t  = ((uint16_t)buf[0] << 8U) | buf[1];
    uint16_t raw_rh = ((uint16_t)buf[3] << 8U) | buf[4];

    data->temperature = -45.0f + 175.0f * ((float)raw_t  / 65535.0f);

    rh = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);
    if      (rh <   0.0f) { rh =   0.0f; }
    else if (rh > 100.0f) { rh = 100.0f; }
    data->humidity = rh;

    return SHT45_OK;
}
