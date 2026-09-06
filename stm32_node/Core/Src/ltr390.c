/**
 ******************************************************************************
 * @file    ltr390.c
 * @brief   LTR390 UV/ALS sensor driver
 *
 * References used for register map and init sequence:
 * - LTR-390UV-01 datasheet
 * - Adafruit_LTR390 implementation (mode/gain/resolution handling)
 ******************************************************************************
 */

#include "ltr390.h"
#include "i2c.h"
#include "stm32wlxx_hal.h"

#define LTR390_I2C_ADDR_7B       (0x53U)

#define LTR390_REG_MAIN_CTRL     (0x00U)
#define LTR390_REG_MEAS_RATE     (0x04U)
#define LTR390_REG_GAIN          (0x05U)
#define LTR390_REG_PART_ID       (0x06U)
#define LTR390_REG_MAIN_STATUS   (0x07U)
#define LTR390_REG_UVS_DATA      (0x10U)

#define LTR390_PART_ID_NIBBLE    (0x0BU)

#define LTR390_CTRL_EN_BIT       (1U << 1)
#define LTR390_CTRL_MODE_BIT     (1U << 3) /* 0: ALS, 1: UVS */
#define LTR390_CTRL_RESET_BIT    (1U << 4)

#define LTR390_STATUS_DATA_RDY   (1U << 3)

#define LTR390_GAIN_X3           (1U)      /* per Adafruit enums */
#define LTR390_RES_16BIT         (4U)      /* per Adafruit enums, in bits [6:4] */
#define LTR390_RATE_100MS        (2U)      /* typical 100 ms measurement rate */

/* UVI = raw / counts_per_uvi,  counts_per_uvi = 2300 * (gain/18) * (integ_ms/400)
 * Baseline per LTR390 datasheet (confirmed by Linux kernel driver):
 *   2300 counts/UVI at gain=18, 20-bit resolution (400 ms integration).
 * For gain x3, 16-bit (25 ms):
 *   counts_per_uvi = 2300 * 3/18 * 25/400 = 172500/7200 = 23.96 -> 24
 * Result scaled x100 to keep two decimal places without float.
 *
 * 2mm PS (polystyrene) window correction measured empirically:
 *   UVI_bare / UVI_through_glass = 6.8 / 5.2 = 17/13
 * Applied as integer fraction: multiply by 17, divide by 13.
 * Remove LTR390_WINDOW_CORR_* when UV-transparent acrylic installed. */
#define LTR390_UVI_DIVISOR       (24U)
#define LTR390_WINDOW_CORR_NUM   (17U)
#define LTR390_WINDOW_CORR_DEN   (13U)
#define LTR390_UVI_SCALE         (100U)
#define LTR390_UVI_X100_MAX      (0xFFFFU)

#define LTR390_I2C_TIMEOUT_MS    (50U)
#define LTR390_RESET_DELAY_MS    (10U)
#define LTR390_BOOT_DELAY_MS     (5U)
#define LTR390_READY_TIMEOUT_MS  (130U)

static uint8_t s_initialised = 0U;

static HAL_StatusTypeDef ltr390_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c2,
                            (uint16_t)(LTR390_I2C_ADDR_7B << 1U),
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            LTR390_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef ltr390_write_reg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c2,
                             (uint16_t)(LTR390_I2C_ADDR_7B << 1U),
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &value,
                             1U,
                             LTR390_I2C_TIMEOUT_MS);
}

static int32_t ltr390_update_bits(uint8_t reg, uint8_t clear_mask, uint8_t set_mask)
{
    uint8_t v;

    if (ltr390_read_regs(reg, &v, 1U) != HAL_OK)
    {
        return LTR390_ERR_I2C;
    }

    v = (uint8_t)((v & (uint8_t)(~clear_mask)) | set_mask);

    if (ltr390_write_reg(reg, v) != HAL_OK)
    {
        return LTR390_ERR_I2C;
    }

    return LTR390_OK;
}

static int32_t ltr390_enable(uint8_t enable)
{
    return ltr390_update_bits(LTR390_REG_MAIN_CTRL,
                              LTR390_CTRL_EN_BIT,
                              (enable != 0U) ? LTR390_CTRL_EN_BIT : 0U);
}

static int32_t ltr390_set_uv_mode(void)
{
    return ltr390_update_bits(LTR390_REG_MAIN_CTRL,
                              LTR390_CTRL_MODE_BIT,
                              LTR390_CTRL_MODE_BIT);
}

static int32_t ltr390_set_gain(uint8_t gain)
{
    return ltr390_update_bits(LTR390_REG_GAIN, 0x07U, (uint8_t)(gain & 0x07U));
}

static int32_t ltr390_set_resolution_and_rate(uint8_t resolution, uint8_t rate)
{
    uint8_t set_mask = (uint8_t)(((resolution & 0x07U) << 4U) | (rate & 0x07U));
    return ltr390_update_bits(LTR390_REG_MEAS_RATE, (uint8_t)(0x70U | 0x07U), set_mask);
}

int32_t LTR390_Init(void)
{
    uint8_t part_id;

    if (ltr390_read_regs(LTR390_REG_PART_ID, &part_id, 1U) != HAL_OK)
    {
        s_initialised = 0U;
        return LTR390_ERR_I2C;
    }

    if ((part_id >> 4U) != LTR390_PART_ID_NIBBLE)
    {
        s_initialised = 0U;
        return LTR390_ERR_ID;
    }

    /* Soft reset (Adafruit notes possible transient bus effects after this). */
    (void)ltr390_write_reg(LTR390_REG_MAIN_CTRL, LTR390_CTRL_RESET_BIT);
    HAL_Delay(LTR390_RESET_DELAY_MS);
    MX_I2C2_Init();
    HAL_Delay(LTR390_BOOT_DELAY_MS);

    if (ltr390_enable(1U) != LTR390_OK)
    {
        s_initialised = 0U;
        return LTR390_ERR_I2C;
    }

    if (ltr390_set_uv_mode() != LTR390_OK)
    {
        s_initialised = 0U;
        return LTR390_ERR_I2C;
    }

    if (ltr390_set_gain(LTR390_GAIN_X3) != LTR390_OK)
    {
        s_initialised = 0U;
        return LTR390_ERR_I2C;
    }

    if (ltr390_set_resolution_and_rate(LTR390_RES_16BIT, LTR390_RATE_100MS) != LTR390_OK)
    {
        s_initialised = 0U;
        return LTR390_ERR_I2C;
    }

    if (ltr390_enable(0U) != LTR390_OK)
    {
        s_initialised = 0U;
        return LTR390_ERR_I2C;
    }

    s_initialised = 1U;
    return LTR390_OK;
}

int32_t LTR390_ReadUV(LTR390_Data_t *data)
{
    uint32_t t0;
    uint8_t status_reg;
    uint8_t buf[3];

    if (data == NULL)
    {
        return LTR390_ERR_PARAM;
    }

    if ((s_initialised == 0U) && (LTR390_Init() != LTR390_OK))
    {
        return LTR390_ERR_INIT;
    }

    if (ltr390_set_uv_mode() != LTR390_OK)
    {
        return LTR390_ERR_I2C;
    }

    if (ltr390_enable(1U) != LTR390_OK)
    {
        return LTR390_ERR_I2C;
    }

    t0 = HAL_GetTick();
    do
    {
        if (ltr390_read_regs(LTR390_REG_MAIN_STATUS, &status_reg, 1U) != HAL_OK)
        {
            (void)ltr390_enable(0U);
            return LTR390_ERR_I2C;
        }

        if ((status_reg & LTR390_STATUS_DATA_RDY) != 0U)
        {
            break;
        }
    } while ((HAL_GetTick() - t0) < LTR390_READY_TIMEOUT_MS);

    if ((status_reg & LTR390_STATUS_DATA_RDY) == 0U)
    {
        (void)ltr390_enable(0U);
        return LTR390_ERR_TIMEOUT;
    }

    if (ltr390_read_regs(LTR390_REG_UVS_DATA, buf, 3U) != HAL_OK)
    {
        (void)ltr390_enable(0U);
        return LTR390_ERR_I2C;
    }

    (void)ltr390_enable(0U);

    uint32_t raw = (((uint32_t)buf[0])
                  | ((uint32_t)buf[1] << 8U)
                  | ((uint32_t)buf[2] << 16U)) & 0x000FFFFFU;

    uint32_t uvi_x100 = (raw * LTR390_UVI_SCALE * LTR390_WINDOW_CORR_NUM) / (LTR390_UVI_DIVISOR * LTR390_WINDOW_CORR_DEN);
    data->uvi_x100 = (uint16_t)(uvi_x100 > LTR390_UVI_X100_MAX ? LTR390_UVI_X100_MAX : uvi_x100);

    return LTR390_OK;
}
