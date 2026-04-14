/**
 ******************************************************************************
 * @file    bmp390.c
 * @brief   BMP390 barometric pressure sensor driver
 *
 * Protocol summary (BMP390 datasheet rev 1.5):
 *   I2C address: 0x76 (SDO = GND) / 0x77 (SDO = VDDIO)
 *   Register map (all registers use 8-bit addressing):
 *     0x00  CHIP_ID    — expected 0x60
 *     0x03  STATUS     — bit5: drdy_press, bit6: drdy_temp
 *     0x04  PRESS_XLSB — pressure [7:0]
 *     0x05  PRESS_LSB  — pressure [15:8]
 *     0x06  PRESS_MSB  — pressure [23:16]  (24-bit raw pressure)
 *     0x07  TEMP_XLSB  — temperature [7:0]
 *     0x08  TEMP_LSB   — temperature [15:8]
 *     0x09  TEMP_MSB   — temperature [23:16] (24-bit raw temperature)
 *     0x1B  PWR_CTRL   — bit0: press_en, bit1: temp_en, bits5:4: mode
 *     0x1C  OSR        — bits2:0: osr_p, bits5:3: osr_t
 *     0x1F  CONFIG     — bits3:1: iir_filter
 *     0x7E  CMD        — 0xB6 = soft reset
 *     0x31–0x45 — 21-byte NVM calibration block
 *
 * Forced mode (mode[1:0] = 01):
 *   Write PWR_CTRL = 0x13 (press_en=1, temp_en=1, mode=01).
 *   Sensor completes one measurement then returns to Sleep automatically.
 *   Max measurement time with osrs_p=×1, osrs_t=×1: 5.70 ms (→ wait 7 ms).
 *
 * Compensation (BMP390 datasheet Appendix A):
 *   NVM coefficients are converted to float during init.
 *   Temperature and pressure compensation follow the formulas in Appendix A.
 *   Output: temperature in °C, pressure in Pa → converted to hPa for output.
 ******************************************************************************
 */

#include "bmp390.h"
#include "i2c.h"
#include "stm32wlxx_hal.h"

/* -------------------------------------------------------------------------- */
/* Private constants                                                          */
/* -------------------------------------------------------------------------- */
#define BMP390_I2C_ADDR0     (0x76U)   /* SDO tied to GND                    */
#define BMP390_I2C_ADDR1     (0x77U)   /* SDO tied to VDDIO                  */
#define BMP390_CHIP_ID_VAL   (0x60U)

/* Register addresses */
#define BMP390_REG_CHIP_ID   (0x00U)
#define BMP390_REG_STATUS    (0x03U)
#define BMP390_REG_PRESS_XLSB (0x04U)
#define BMP390_REG_PWR_CTRL  (0x1BU)
#define BMP390_REG_OSR       (0x1CU)
#define BMP390_REG_CONFIG    (0x1FU)
#define BMP390_REG_CMD       (0x7EU)
#define BMP390_REG_CALIB     (0x31U)  /* first calibration byte              */

/* PWR_CTRL value: press_en(bit0)=1, temp_en(bit1)=1, mode(bits5:4)=01 */
#define BMP390_PWR_CTRL_FORCED  (0x13U)

/* OSR: osr_p=000 (×1), osr_t=000 (×1) */
#define BMP390_OSR_ULP          (0x00U)

/* CONFIG: iir_filter=000 (off) */
#define BMP390_CONFIG_IIR_OFF   (0x00U)

/* Soft-reset command */
#define BMP390_CMD_SOFTRESET    (0xB6U)

/* Timing */
#define BMP390_RESET_DELAY_MS   (3U)
#define BMP390_MEASURE_DELAY_MS (7U)   /* > 5.70 ms max for ULP mode         */
#define BMP390_I2C_TIMEOUT_MS   (50U)
#define BMP390_DRDY_TIMEOUT_MS  (20U)
#define BMP390_RECOVERY_DELAY_MS (2U)

/* Calibration block size (0x31–0x45 inclusive) */
#define BMP390_CALIB_LEN        (21U)

/* -------------------------------------------------------------------------- */
/* Private calibration data                                                   */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* Temperature coefficients (converted to float from NVM) */
    float par_t1;
    float par_t2;
    float par_t3;

    /* Pressure coefficients */
    float par_p1;
    float par_p2;
    float par_p3;
    float par_p4;
    float par_p5;
    float par_p6;
    float par_p7;
    float par_p8;
    float par_p9;
    float par_p10;
    float par_p11;

    /* Compensated temperature; updated by compensate_temperature()
       and used in compensate_pressure() */
    float t_lin;
} BMP390_Calib_t;

static BMP390_Calib_t s_calib;
static uint8_t        s_initialised = 0U;
static uint8_t        s_i2c_addr_7b = BMP390_I2C_ADDR0;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                            */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef bmp390_write_reg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c2,
                             (uint16_t)(s_i2c_addr_7b << 1U),
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &value, 1U,
                             BMP390_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef bmp390_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c2,
                            (uint16_t)(s_i2c_addr_7b << 1U),
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf, len,
                            BMP390_I2C_TIMEOUT_MS);
}

static int32_t bmp390_recover_i2c_and_retry_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    MX_I2C2_Init();
    HAL_Delay(BMP390_RECOVERY_DELAY_MS);

    if (bmp390_read_regs(reg, buf, len) != HAL_OK)
    {
        return BMP390_ERR_I2C;
    }

    return BMP390_OK;
}

static int32_t bmp390_recover_i2c_and_retry_write(uint8_t reg, uint8_t value)
{
    MX_I2C2_Init();
    HAL_Delay(BMP390_RECOVERY_DELAY_MS);

    if (bmp390_write_reg(reg, value) != HAL_OK)
    {
        return BMP390_ERR_I2C;
    }

    return BMP390_OK;
}

/**
 * @brief Parse 21 NVM calibration bytes and convert to float coefficients.
 *        Conversion factors from BMP390 datasheet, Appendix A (Table 11).
 */
static void bmp390_parse_calib(const uint8_t *raw)
{
    uint16_t nvm_t1  = (uint16_t)raw[0]  | ((uint16_t)raw[1]  << 8U);
    uint16_t nvm_t2  = (uint16_t)raw[2]  | ((uint16_t)raw[3]  << 8U);
    int8_t   nvm_t3  = (int8_t)raw[4];

    int16_t  nvm_p1  = (int16_t)((uint16_t)raw[5]  | ((uint16_t)raw[6]  << 8U));
    int16_t  nvm_p2  = (int16_t)((uint16_t)raw[7]  | ((uint16_t)raw[8]  << 8U));
    int8_t   nvm_p3  = (int8_t)raw[9];
    int8_t   nvm_p4  = (int8_t)raw[10];
    uint16_t nvm_p5  = (uint16_t)raw[11] | ((uint16_t)raw[12] << 8U);
    uint16_t nvm_p6  = (uint16_t)raw[13] | ((uint16_t)raw[14] << 8U);
    int8_t   nvm_p7  = (int8_t)raw[15];
    int8_t   nvm_p8  = (int8_t)raw[16];
    int16_t  nvm_p9  = (int16_t)((uint16_t)raw[17] | ((uint16_t)raw[18] << 8U));
    int8_t   nvm_p10 = (int8_t)raw[19];
    int8_t   nvm_p11 = (int8_t)raw[20];

    /* Temperature — Appendix A, Table 11 */
    s_calib.par_t1 = (float)nvm_t1 * 256.0f;              /* / 2^-8          */
    s_calib.par_t2 = (float)nvm_t2 / 1073741824.0f;       /* / 2^30          */
    s_calib.par_t3 = (float)nvm_t3 / 281474976710656.0f;  /* / 2^48          */

    /* Pressure — Appendix A, Table 11 */
    s_calib.par_p1  = ((float)nvm_p1  - 16384.0f) / 1048576.0f;    /* (n-2^14)/2^20  */
    s_calib.par_p2  = ((float)nvm_p2  - 16384.0f) / 536870912.0f;  /* (n-2^14)/2^29  */
    s_calib.par_p3  = (float)nvm_p3  / 4294967296.0f;              /* / 2^32         */
    s_calib.par_p4  = (float)nvm_p4  / 137438953472.0f;            /* / 2^37         */
    s_calib.par_p5  = (float)nvm_p5  * 8.0f;                       /* / 2^-3         */
    s_calib.par_p6  = (float)nvm_p6  / 64.0f;                      /* / 2^6          */
    s_calib.par_p7  = (float)nvm_p7  / 256.0f;                     /* / 2^8          */
    s_calib.par_p8  = (float)nvm_p8  / 32768.0f;                   /* / 2^15         */
    s_calib.par_p9  = (float)nvm_p9  / 281474976710656.0f;         /* / 2^48         */
    s_calib.par_p10 = (float)nvm_p10 / 281474976710656.0f;         /* / 2^48         */
    s_calib.par_p11 = (float)nvm_p11 / 36893488147419103232.0f;    /* / 2^65         */
}

/**
 * @brief Compensate raw temperature ADC value.
 *        Updates s_calib.t_lin (used by compensate_pressure).
 * @return Compensated temperature in °C.
 */
static float compensate_temperature(uint32_t uncomp_t)
{
    float pd1 = (float)uncomp_t - s_calib.par_t1;
    float pd2 = pd1 * s_calib.par_t2;
    s_calib.t_lin = pd2 + pd1 * pd1 * s_calib.par_t3;
    return s_calib.t_lin;
}

/**
 * @brief Compensate raw pressure ADC value.
 *        Must be called after compensate_temperature (needs s_calib.t_lin).
 * @return Compensated pressure in Pa.
 */
static float compensate_pressure(uint32_t uncomp_p)
{
    float t   = s_calib.t_lin;
    float t2  = t * t;
    float t3  = t2 * t;

    float po1 = s_calib.par_p5
              + s_calib.par_p6 * t
              + s_calib.par_p7 * t2
              + s_calib.par_p8 * t3;

    float po2 = (float)uncomp_p
              * (s_calib.par_p1
                 + s_calib.par_p2 * t
                 + s_calib.par_p3 * t2
                 + s_calib.par_p4 * t3);

    float p2  = (float)uncomp_p * (float)uncomp_p;
    float p3  = p2 * (float)uncomp_p;

    float po3 = p2 * (s_calib.par_p9 + s_calib.par_p10 * t)
              + p3 * s_calib.par_p11;

    return po1 + po2 + po3;  /* in Pa */
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

int32_t BMP390_Init(void)
{
    HAL_StatusTypeDef status;
    uint8_t val;
    uint8_t calib_raw[BMP390_CALIB_LEN];

    /* Probe both legal BMP390 I2C addresses once at startup. */
    s_i2c_addr_7b = BMP390_I2C_ADDR0;
    status = bmp390_read_regs(BMP390_REG_CHIP_ID, &val, 1U);
    if ((status != HAL_OK) || (val != BMP390_CHIP_ID_VAL))
    {
        s_i2c_addr_7b = BMP390_I2C_ADDR1;
        status = bmp390_read_regs(BMP390_REG_CHIP_ID, &val, 1U);
        if ((status != HAL_OK) || (val != BMP390_CHIP_ID_VAL))
        {
            s_initialised = 0U;
            s_i2c_addr_7b = BMP390_I2C_ADDR0;
            return (status != HAL_OK) ? BMP390_ERR_I2C : BMP390_ERR_ID;
        }
    }

    /* 1. Soft reset --------------------------------------------------------- */
    val = BMP390_CMD_SOFTRESET;
    status = bmp390_write_reg(BMP390_REG_CMD, val);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_write(BMP390_REG_CMD, val) != BMP390_OK)
        {
            s_initialised = 0U;
            return BMP390_ERR_I2C;
        }
    }
    HAL_Delay(BMP390_RESET_DELAY_MS);

    /* 2. Verify CHIP_ID ----------------------------------------------------- */
    status = bmp390_read_regs(BMP390_REG_CHIP_ID, &val, 1U);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_read(BMP390_REG_CHIP_ID, &val, 1U) != BMP390_OK)
        {
            s_initialised = 0U;
            return BMP390_ERR_I2C;
        }
    }
    if (val != BMP390_CHIP_ID_VAL)
    {
        s_initialised = 0U;
        return BMP390_ERR_ID;
    }

    /* 3. Read NVM calibration ----------------------------------------------- */
    status = bmp390_read_regs(BMP390_REG_CALIB, calib_raw, BMP390_CALIB_LEN);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_read(BMP390_REG_CALIB, calib_raw, BMP390_CALIB_LEN) != BMP390_OK)
        {
            s_initialised = 0U;
            return BMP390_ERR_I2C;
        }
    }
    bmp390_parse_calib(calib_raw);

    /* 4. Configure OSR (×1/×1) and IIR (off) -------------------------------- */
    status = bmp390_write_reg(BMP390_REG_OSR,    BMP390_OSR_ULP);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_write(BMP390_REG_OSR, BMP390_OSR_ULP) != BMP390_OK)
        {
            s_initialised = 0U;
            return BMP390_ERR_I2C;
        }
    }
    status = bmp390_write_reg(BMP390_REG_CONFIG,  BMP390_CONFIG_IIR_OFF);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_write(BMP390_REG_CONFIG, BMP390_CONFIG_IIR_OFF) != BMP390_OK)
        {
            s_initialised = 0U;
            return BMP390_ERR_I2C;
        }
    }

    /* Sensor stays in Sleep mode (≈1.4 µA) until BMP390_Read() is called.   */
    s_initialised = 1U;
    return BMP390_OK;
}

int32_t BMP390_Read(BMP390_Data_t *data)
{
    HAL_StatusTypeDef status;
    uint32_t t0;
    uint8_t status_reg;
    uint8_t buf[6];

    if (data == NULL)
    {
        return BMP390_ERR_PARAM;
    }

    if (s_initialised == 0U)
    {
        return BMP390_ERR_INIT;
    }

    /* 1. Trigger Forced-mode measurement ------------------------------------ */
    status = bmp390_write_reg(BMP390_REG_PWR_CTRL, BMP390_PWR_CTRL_FORCED);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_write(BMP390_REG_PWR_CTRL, BMP390_PWR_CTRL_FORCED) != BMP390_OK)
        {
            return BMP390_ERR_I2C;
        }
    }

    /* 2. Wait for measurement to complete (max 5.70 ms → 7 ms margin) ------- */
    HAL_Delay(BMP390_MEASURE_DELAY_MS);
    t0 = HAL_GetTick();
    do
    {
        status = bmp390_read_regs(BMP390_REG_STATUS, &status_reg, 1U);
        if (status != HAL_OK)
        {
            if (bmp390_recover_i2c_and_retry_read(BMP390_REG_STATUS, &status_reg, 1U) != BMP390_OK)
            {
                return BMP390_ERR_I2C;
            }
        }

        if ((status_reg & 0x60U) == 0x60U)
        {
            break;
        }
    } while ((HAL_GetTick() - t0) < BMP390_DRDY_TIMEOUT_MS);

    if ((status_reg & 0x60U) != 0x60U)
    {
        return BMP390_ERR_I2C;
    }

    /* 3. Burst-read pressure + temperature (0x04–0x09, 6 bytes) ------------ */
    status = bmp390_read_regs(BMP390_REG_PRESS_XLSB, buf, 6U);
    if (status != HAL_OK)
    {
        if (bmp390_recover_i2c_and_retry_read(BMP390_REG_PRESS_XLSB, buf, 6U) != BMP390_OK)
        {
            return BMP390_ERR_I2C;
        }
    }

    /* Sensor is back in Sleep mode now (Forced mode auto-returns).           */

    /* 4. Reconstruct 24-bit raw values (XLSB / LSB / MSB order) ------------ */
    uint32_t raw_p = (uint32_t)buf[0]
                   | ((uint32_t)buf[1] << 8U)
                   | ((uint32_t)buf[2] << 16U);

    uint32_t raw_t = (uint32_t)buf[3]
                   | ((uint32_t)buf[4] << 8U)
                   | ((uint32_t)buf[5] << 16U);

    /* 5. Compensate — temperature must be computed first (updates t_lin) --- */
    data->temperature    = compensate_temperature(raw_t);
    data->pressure_hPa   = compensate_pressure(raw_p) / 100.0f; /* Pa → hPa */

    return BMP390_OK;
}
