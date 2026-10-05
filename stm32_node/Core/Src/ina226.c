/**
 ******************************************************************************
 * @file    ina226.c
 * @brief   INA226 driver: triggered measurements, powered down in between
 *
 * Calibration (datasheet eq. 1): Current_LSB = 0.1 mA,
 *   CAL = 0.00512 / (Current_LSB * R_shunt) = 0.00512 / (1e-4 * 0.068) = 753.
 * Shunt range +-81.92 mV -> 1.2 A maximum. Bus LSB 1.25 mV.
 * Power is computed by the caller from V x I; the power register
 * (25 x Current_LSB = 2.5 mW) is too coarse.
 ******************************************************************************
 */

#include "ina226.h"
#include "i2c.h"
#include "stm32wlxx_hal.h"

#define INA226_REG_CONFIG       (0x00U)
#define INA226_REG_BUS          (0x02U)
#define INA226_REG_CURRENT      (0x04U)
#define INA226_REG_CAL          (0x05U)
#define INA226_REG_MASK_ENABLE  (0x06U)
#define INA226_REG_MANUFACTURER (0xFEU)
#define INA226_REG_DIE_ID       (0xFFU)

#define INA226_MANUFACTURER_TI  (0x5449U)
#define INA226_DIE_ID_MASK      (0xFFF0U)
#define INA226_DIE_ID           (0x2260U)

#define INA226_CAL_68MOHM       (753U)

/* CONFIG: bit 14 always reads 1. AVG = 16 (010), VBUSCT = VSHCT = 1.1 ms
 * (100): 16 x (1.1 + 1.1) ms = 35.2 ms per triggered measurement. */
#define INA226_CFG_BASE         (0x4000U | (2U << 9) | (4U << 6) | (4U << 3))
#define INA226_MODE_POWER_DOWN  (0x0U)
#define INA226_MODE_TRIG_BOTH   (0x3U)
#define INA226_CONVERSION_MS    (36U)
#define INA226_READY_POLLS      (10U)
#define INA226_CVRF             (1U << 3)

#define INA226_I2C_TIMEOUT_MS   (50U)

static uint8_t s_initialised = 0U;
static uint32_t s_last_error = 0U;

static int32_t ina226_read(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    if (HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(INA226_I2C_ADDR_7B << 1U), reg, I2C_MEMADD_SIZE_8BIT,
                         buf, 2U, INA226_I2C_TIMEOUT_MS) != HAL_OK)
    {
        s_last_error = HAL_I2C_GetError(&hi2c2);
        return INA226_ERR_I2C;
    }
    *value = (uint16_t)(((uint16_t)buf[0] << 8U) | buf[1]);
    return INA226_OK;
}

static int32_t ina226_write(uint8_t reg, uint16_t value)
{
    uint8_t buf[2] = { (uint8_t)(value >> 8), (uint8_t)value };
    if (HAL_I2C_Mem_Write(&hi2c2, (uint16_t)(INA226_I2C_ADDR_7B << 1U), reg, I2C_MEMADD_SIZE_8BIT,
                          buf, 2U, INA226_I2C_TIMEOUT_MS) != HAL_OK)
    {
        s_last_error = HAL_I2C_GetError(&hi2c2);
        return INA226_ERR_I2C;
    }
    return INA226_OK;
}

int32_t INA226_Init(void)
{
    uint16_t manufacturer, die;

    s_initialised = 0U;
    if (ina226_read(INA226_REG_MANUFACTURER, &manufacturer) != INA226_OK ||
        ina226_read(INA226_REG_DIE_ID, &die) != INA226_OK)
    {
        return INA226_ERR_I2C;
    }
    if (manufacturer != INA226_MANUFACTURER_TI || (die & INA226_DIE_ID_MASK) != INA226_DIE_ID)
    {
        return INA226_ERR_INIT;
    }
    if (ina226_write(INA226_REG_CAL, INA226_CAL_68MOHM) != INA226_OK ||
        INA226_Shutdown() != INA226_OK)
    {
        return INA226_ERR_I2C;
    }
    s_initialised = 1U;
    s_last_error = 0U;
    return INA226_OK;
}

int32_t INA226_Shutdown(void)
{
    return ina226_write(INA226_REG_CONFIG, INA226_CFG_BASE | INA226_MODE_POWER_DOWN);
}

int32_t INA226_Measure(INA226_Data_t *data)
{
    uint16_t flags = 0U, bus, current;

    if (data == NULL)
    {
        return INA226_ERR_PARAM;
    }
    if (s_initialised == 0U)
    {
        return INA226_ERR_INIT;
    }
    /* Reading MASK/ENABLE clears a stale CVRF. The calibration is rewritten
     * each time: it costs one transfer and survives a brown-out of the part. */
    if (ina226_read(INA226_REG_MASK_ENABLE, &flags) != INA226_OK ||
        ina226_write(INA226_REG_CAL, INA226_CAL_68MOHM) != INA226_OK ||
        ina226_write(INA226_REG_CONFIG, INA226_CFG_BASE | INA226_MODE_TRIG_BOTH) != INA226_OK)
    {
        return INA226_ERR_I2C;
    }
    HAL_Delay(INA226_CONVERSION_MS);
    for (uint8_t i = 0U; ; ++i)
    {
        if (ina226_read(INA226_REG_MASK_ENABLE, &flags) != INA226_OK)
        {
            (void)INA226_Shutdown();
            return INA226_ERR_I2C;
        }
        if ((flags & INA226_CVRF) != 0U) break;
        if (i >= INA226_READY_POLLS)
        {
            (void)INA226_Shutdown();
            return INA226_ERR_TIMEOUT;
        }
        HAL_Delay(1U);
    }
    int32_t status = INA226_OK;
    if (ina226_read(INA226_REG_BUS, &bus) != INA226_OK ||
        ina226_read(INA226_REG_CURRENT, &current) != INA226_OK)
    {
        status = INA226_ERR_I2C;
    }
    /* The datasheet does not promise an automatic power-down after a
     * triggered conversion; power down explicitly to reach the ~2 uA floor. */
    if (INA226_Shutdown() != INA226_OK)
    {
        status = INA226_ERR_I2C;
    }
    if (status != INA226_OK)
    {
        return status;
    }
    data->bus_mv = (uint16_t)(((uint32_t)bus * 5U) / 4U);
    /* D1 blocks reverse current; a negative reading is offset noise. */
    data->current_01ma = ((int16_t)current < 0) ? 0U : current;
    return INA226_OK;
}

uint32_t INA226_LastI2cError(void)
{
    return s_last_error;
}
