/**
 ******************************************************************************
 * @file    max17048.c
 * @brief   MAX17048 fuel gauge driver
 ******************************************************************************
 */

#include "max17048.h"
#include "i2c.h"
#include "stm32wlxx_hal.h"

#define MAX17048_I2C_ADDR_7B     (0x36U)

#define MAX17048_REG_VCELL       (0x02U)
#define MAX17048_REG_VERSION     (0x08U)

#define MAX17048_I2C_TIMEOUT_MS  (50U)

/* VCELL LSB = 78.125 uV -> 0.000078125 V */
/* VCELL LSB = 78.125 uV = 78125 nV = 5/64 mV.
 * mV = raw * 5 / 64. Max raw = 65535 -> 65535 * 5 = 327675 < 2^31 (int32 ok). */
#define MAX17048_VCELL_MV_NUM     (5U)
#define MAX17048_VCELL_MV_DEN     (64U)

static uint8_t s_initialised = 0U;

static HAL_StatusTypeDef max17048_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c2,
                            (uint16_t)(MAX17048_I2C_ADDR_7B << 1U),
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            MAX17048_I2C_TIMEOUT_MS);
}

int32_t MAX17048_Init(void)
{
    uint8_t version[2];

    if (HAL_I2C_IsDeviceReady(&hi2c2,
                              (uint16_t)(MAX17048_I2C_ADDR_7B << 1U),
                              2U,
                              MAX17048_I2C_TIMEOUT_MS) != HAL_OK)
    {
        s_initialised = 0U;
        return MAX17048_ERR_I2C;
    }

    if (max17048_read_regs(MAX17048_REG_VERSION, version, 2U) != HAL_OK)
    {
        s_initialised = 0U;
        return MAX17048_ERR_I2C;
    }

    s_initialised = 1U;
    return MAX17048_OK;
}

int32_t MAX17048_Read(MAX17048_Data_t *data)
{
    uint8_t raw[2];
    uint16_t raw16;

    if (data == NULL)
    {
        return MAX17048_ERR_PARAM;
    }

    if (s_initialised == 0U)
    {
        return MAX17048_ERR_INIT;
    }

    if (max17048_read_regs(MAX17048_REG_VCELL, raw, 2U) != HAL_OK)
    {
        return MAX17048_ERR_I2C;
    }

    raw16 = (uint16_t)(((uint16_t)raw[0] << 8U) | (uint16_t)raw[1]);

    /* Integer math: mV = (raw * 5 + 32) / 64 (rounded). */
    uint32_t mv = ((uint32_t)raw16 * MAX17048_VCELL_MV_NUM
                   + (MAX17048_VCELL_MV_DEN / 2U)) / MAX17048_VCELL_MV_DEN;
    if (mv > 0xFFFFU) { mv = 0xFFFFU; }
    if (mv < 1800U || mv > 5000U) return MAX17048_ERR_I2C;
    data->voltage_mv = (uint16_t)mv;

    return MAX17048_OK;
}
