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
#define MAX17048_REG_SOC         (0x04U)
#define MAX17048_REG_VERSION     (0x08U)
#define MAX17048_REG_HIBRT       (0x0AU)
#define MAX17048_REG_CONFIG      (0x0CU)
#define MAX17048_REG_CRATE       (0x16U)
#define MAX17048_REG_STATUS      (0x1AU)

/* Always hibernate: 3 uA instead of 23 uA, one conversion every 45 s. */
#define MAX17048_HIBRT_ALWAYS    (0xFFFFU)
/* CONFIG low byte: ALSC (bit 6) and ALRT (bit 5), ATHD in bits 4..0 with
 * threshold = 32 % - ATHD; 0x1F is the lowest alarm threshold (1 %). */
#define MAX17048_CONFIG_ALSC     (1U << 6)
#define MAX17048_CONFIG_ALRT     (1U << 5)
#define MAX17048_CONFIG_ATHD_MSK (0x1FU)
/* STATUS high byte alert flags RI, VH, VL, VR, HD, SC (bits 8..13); EnVR
 * (bit 14) is a setting, not a flag. Flags clear by writing 0. */
#define MAX17048_STATUS_FLAGS    (0x3F00U)

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

/* Every register is 16 bit wide; 8-bit writes have no effect. */
static HAL_StatusTypeDef max17048_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t buf[2] = { (uint8_t)(value >> 8), (uint8_t)value };
    return HAL_I2C_Mem_Write(&hi2c2,
                             (uint16_t)(MAX17048_I2C_ADDR_7B << 1U),
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             buf,
                             2U,
                             MAX17048_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef max17048_read_reg(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef st = max17048_read_regs(reg, buf, 2U);
    *value = (uint16_t)(((uint16_t)buf[0] << 8U) | (uint16_t)buf[1]);
    return st;
}

/* SOC LSB is 1/256 %; 0.01 % = raw * 100 / 256 = raw * 25 / 64. */
static uint16_t max17048_soc_x100(uint16_t raw)
{
    return (uint16_t)((uint32_t)raw * 25U / 64U);
}

/* CRATE LSB is 0.208 %/h, signed; 0.01 %/h = raw * 20.8. The result is
 * clamped to +-32767 so it never collides with the 0x8000 sentinel. */
static int16_t max17048_crate_x100(uint16_t raw)
{
    int32_t v = (int32_t)(int16_t)raw * 104 / 5;
    if (v > 32767) { v = 32767; }
    if (v < -32767) { v = -32767; }
    return (int16_t)v;
}

/* Clear a pending alert: with ALRT set the open-drain output holds PA0 low
 * and pulls about 33 uA through the 100 kOhm pull-up. Disables the 1 %
 * change alert and sets the SOC alarm to its minimum. */
static HAL_StatusTypeDef max17048_clear_alert(void)
{
    uint16_t config, status;
    if (max17048_read_reg(MAX17048_REG_CONFIG, &config) != HAL_OK) { return HAL_ERROR; }
    uint16_t wanted = (uint16_t)((config & ~(MAX17048_CONFIG_ALSC | MAX17048_CONFIG_ALRT))
                                 | MAX17048_CONFIG_ATHD_MSK);
    if (wanted != config && max17048_write_reg(MAX17048_REG_CONFIG, wanted) != HAL_OK) { return HAL_ERROR; }
    if (max17048_read_reg(MAX17048_REG_STATUS, &status) != HAL_OK) { return HAL_ERROR; }
    if ((status & MAX17048_STATUS_FLAGS) != 0U &&
        max17048_write_reg(MAX17048_REG_STATUS, (uint16_t)(status & ~MAX17048_STATUS_FLAGS)) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_OK;
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

    if (max17048_read_regs(MAX17048_REG_VERSION, version, 2U) != HAL_OK ||
        max17048_write_reg(MAX17048_REG_HIBRT, MAX17048_HIBRT_ALWAYS) != HAL_OK ||
        max17048_clear_alert() != HAL_OK)
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

int32_t MAX17048_ReadGauge(MAX17048_Gauge_t *gauge)
{
    uint16_t soc, crate;

    if (gauge == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    gauge->soc_x100 = MAX17048_SOC_INVALID;
    gauge->crate_x100 = MAX17048_CRATE_INVALID;
    if (s_initialised == 0U)
    {
        return MAX17048_ERR_INIT;
    }
    /* Re-acknowledge on every block: an alert raised since the last one
     * would otherwise hold ALRT low until the next reset. */
    if (max17048_clear_alert() != HAL_OK ||
        max17048_read_reg(MAX17048_REG_SOC, &soc) != HAL_OK ||
        max17048_read_reg(MAX17048_REG_CRATE, &crate) != HAL_OK)
    {
        return MAX17048_ERR_I2C;
    }
    gauge->soc_x100 = max17048_soc_x100(soc);
    gauge->crate_x100 = max17048_crate_x100(crate);
    return MAX17048_OK;
}
