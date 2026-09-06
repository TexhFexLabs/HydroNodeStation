/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scd41.c
  * @author  MCD Application Team
  * @brief   SCD41 CO2 sensor low-level driver
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_conf.h"
#include "i2c.h"
#include "scd41.h"

/* Private define ------------------------------------------------------------*/
#define SCD41_I2C_ADDR_8BIT                    (0x62U << 1)

#define SCD41_CMD_STOP_PERIODIC_MEASUREMENT    0x3F86U
#define SCD41_CMD_SET_TEMPERATURE_OFFSET       0x241DU
#define SCD41_CMD_SET_SENSOR_ALTITUDE          0x2427U
#define SCD41_CMD_SET_AMBIENT_PRESSURE         0xE000U
#define SCD41_CMD_SET_ASC                       0x2416U
#define SCD41_CMD_PERSIST_SETTINGS             0x3615U
#define SCD41_CMD_READ_MEASUREMENT             0xEC05U
#define SCD41_CMD_GET_DATA_READY_STATUS        0xE4B8U
#define SCD41_CMD_MEASURE_SINGLE_SHOT          0x219DU
#define SCD41_CMD_MEASURE_SINGLE_SHOT_RHT      0x2196U
#define SCD41_CMD_POWER_DOWN                   0x36E0U
#define SCD41_CMD_WAKE_UP                      0x36F6U

#define SCD41_DATA_READY_MASK                  0x07FFU
#define SCD41_DATA_READY_POLL_MS               50U
#define SCD41_DATA_READY_TIMEOUT_MS            1500U

/* Private function prototypes -----------------------------------------------*/
static uint8_t SCD41_CalculateCrc(const uint8_t *data, uint8_t length);
static int32_t SCD41_BusInit(void);
static void SCD41_BusDeInit(void);
static int32_t SCD41_WriteCommand(uint16_t command);
static int32_t SCD41_WriteCommandWithWord(uint16_t command, uint16_t data);
static int32_t SCD41_GetDataReadyStatus(uint16_t *status_word);
static int32_t SCD41_WaitDataReady(uint32_t timeout_ms);
static int32_t SCD41_ReadMeasurementWords(uint16_t *co2_raw, uint16_t *temperature_raw, uint16_t *humidity_raw);
static int16_t  SCD41_ConvertTemperature(uint16_t temperature_raw);
static uint16_t SCD41_ConvertHumidity(uint16_t humidity_raw);

/* Private functions ---------------------------------------------------------*/
static uint8_t SCD41_CalculateCrc(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0xFFU;
  uint8_t i;
  uint8_t bit;

  for (i = 0U; i < length; i++)
  {
    crc ^= data[i];
    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1U) ^ 0x31U);
      }
      else
      {
        crc <<= 1U;
      }
    }
  }

  return crc;
}

static int32_t SCD41_BusInit(void)
{
  /* Use CubeMX-generated I2C2 configuration from i2c.c */
  if ((hi2c2.Instance != I2C2) || (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_RESET))
  {
    MX_I2C2_Init();
  }

  if (hi2c2.Instance != I2C2)
  {
    return SCD41_STATUS_ERROR;
  }

  return SCD41_STATUS_OK;
}

static void SCD41_BusDeInit(void)
{
  /* The application owns the shared bus; leave other sensors connected. */
}

static int32_t SCD41_WriteCommand(uint16_t command)
{
  uint8_t tx[2];

  tx[0] = (uint8_t)(command >> 8);
  tx[1] = (uint8_t)(command & 0xFFU);

  if (HAL_I2C_Master_Transmit(&hi2c2, SCD41_I2C_ADDR_8BIT, tx, sizeof(tx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  return SCD41_STATUS_OK;
}

static int32_t SCD41_WriteCommandWithWord(uint16_t command, uint16_t data)
{
  uint8_t tx[5];

  tx[0] = (uint8_t)(command >> 8);
  tx[1] = (uint8_t)(command & 0xFFU);
  tx[2] = (uint8_t)(data >> 8);
  tx[3] = (uint8_t)(data & 0xFFU);
  tx[4] = SCD41_CalculateCrc(&tx[2], 2U);

  if (HAL_I2C_Master_Transmit(&hi2c2, SCD41_I2C_ADDR_8BIT, tx, sizeof(tx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  return SCD41_STATUS_OK;
}

static int32_t SCD41_GetDataReadyStatus(uint16_t *status_word)
{
  uint8_t rx[3];

  if (status_word == NULL)
  {
    return SCD41_STATUS_ERROR;
  }

  if (SCD41_WriteCommand(SCD41_CMD_GET_DATA_READY_STATUS) != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  HAL_Delay(1U);

  if (HAL_I2C_Master_Receive(&hi2c2, SCD41_I2C_ADDR_8BIT, rx, sizeof(rx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  if (SCD41_CalculateCrc(&rx[0], 2U) != rx[2])
  {
    return SCD41_STATUS_CRC_ERROR;
  }

  *status_word = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);

  return SCD41_STATUS_OK;
}

static int32_t SCD41_WaitDataReady(uint32_t timeout_ms)
{
  (void)timeout_ms;
  uint16_t status_word = 0U;
  int32_t status = SCD41_GetDataReadyStatus(&status_word);
  if (status != SCD41_STATUS_OK) { return status; }
  return (status_word & SCD41_DATA_READY_MASK) ? SCD41_STATUS_OK : SCD41_STATUS_ERROR;
}

static int32_t SCD41_ReadMeasurementWords(uint16_t *co2_raw, uint16_t *temperature_raw, uint16_t *humidity_raw)
{
  uint8_t rx[9];

  if (SCD41_WriteCommand(SCD41_CMD_READ_MEASUREMENT) != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  HAL_Delay(1U);

  if (HAL_I2C_Master_Receive(&hi2c2, SCD41_I2C_ADDR_8BIT, rx, sizeof(rx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  if ((SCD41_CalculateCrc(&rx[0], 2U) != rx[2]) ||
      (SCD41_CalculateCrc(&rx[3], 2U) != rx[5]) ||
      (SCD41_CalculateCrc(&rx[6], 2U) != rx[8]))
  {
    return SCD41_STATUS_CRC_ERROR;
  }

  *co2_raw = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
  *temperature_raw = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);
  *humidity_raw = (uint16_t)(((uint16_t)rx[6] << 8) | rx[7]);

  return SCD41_STATUS_OK;
}

/* T [degC]  = -45 + 175 * raw / 65536,     output = degC * 100 (0.01 degC)
 * RH [%RH] =       100 * raw / 65536,     output = %RH  * 100 (0.01 %RH)
 * Max intermediate: 17500 * 65535 < 2^31 -> int32 safe. */
static int16_t SCD41_ConvertTemperature(uint16_t temperature_raw)
{
  int32_t t = -4500 + (int32_t)(((uint32_t)17500U * (uint32_t)temperature_raw
                                 + 32768U) >> 16);
  if (t >  32767) { t =  32767; }
  if (t < -32768) { t = -32768; }
  return (int16_t)t;
}

static uint16_t SCD41_ConvertHumidity(uint16_t humidity_raw)
{
  uint32_t h = ((uint32_t)10000U * (uint32_t)humidity_raw + 32768U) >> 16;
  if (h > 10000U) { h = 10000U; }
  return (uint16_t)h;
}

/* Exported functions --------------------------------------------------------*/
int32_t SCD41_Init(void)
{
  uint32_t temp_offset_x100 = SCD41_TEMPERATURE_OFFSET_C_X100;

  if (SCD41_BusInit() != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  (void)SCD41_WriteCommand(SCD41_CMD_WAKE_UP);
  HAL_Delay(SCD41_WAKEUP_DELAY_MS);
  (void)SCD41_WriteCommand(SCD41_CMD_STOP_PERIODIC_MEASUREMENT);
  HAL_Delay(500U);

  if (temp_offset_x100 > 0U)
  {
    uint32_t temp_offset_raw = (temp_offset_x100 * 65536U) / 17500U;
    if (temp_offset_raw > 0xFFFFU)
    {
      temp_offset_raw = 0xFFFFU;
    }
    if (SCD41_WriteCommandWithWord(SCD41_CMD_SET_TEMPERATURE_OFFSET, (uint16_t)temp_offset_raw) != SCD41_STATUS_OK)
    {
      SCD41_BusDeInit();
      return SCD41_STATUS_ERROR;
    }
  }

  if (SCD41_SENSOR_ALTITUDE_M > 0U)
  {
    if (SCD41_WriteCommandWithWord(SCD41_CMD_SET_SENSOR_ALTITUDE, (uint16_t)SCD41_SENSOR_ALTITUDE_M) != SCD41_STATUS_OK)
    {
      SCD41_BusDeInit();
      return SCD41_STATUS_ERROR;
    }
  }

  if (SCD41_WriteCommandWithWord(SCD41_CMD_SET_ASC, (uint16_t)(SCD41_ASC_ENABLED != 0U ? 1U : 0U)) != SCD41_STATUS_OK)
  {
    SCD41_BusDeInit();
    return SCD41_STATUS_ERROR;
  }

#if (SCD41_PERSIST_SETTINGS == 1U)
  if (SCD41_WriteCommand(SCD41_CMD_PERSIST_SETTINGS) != SCD41_STATUS_OK)
  {
    SCD41_BusDeInit();
    return SCD41_STATUS_ERROR;
  }
  HAL_Delay(800U);
#endif

  (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN);
  SCD41_BusDeInit();

  return SCD41_STATUS_OK;
}

int32_t SCD41_ReadRhtSingleShot(int16_t *temperature, uint16_t *humidity)
{
  uint16_t co2_raw = 0;
  uint16_t temperature_raw = 0;
  uint16_t humidity_raw = 0;
  int32_t status;

  if ((temperature == NULL) || (humidity == NULL))
  {
    return SCD41_STATUS_ERROR;
  }

  if (SCD41_BusInit() != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  (void)SCD41_WriteCommand(SCD41_CMD_WAKE_UP);
  HAL_Delay(SCD41_WAKEUP_DELAY_MS);

  status = SCD41_WriteCommand(SCD41_CMD_MEASURE_SINGLE_SHOT_RHT);
  if (status != SCD41_STATUS_OK)
  {
    SCD41_BusDeInit();
    return status;
  }

  HAL_Delay(SCD41_SINGLE_SHOT_RHT_WAIT_MS);

  status = SCD41_ReadMeasurementWords(&co2_raw, &temperature_raw, &humidity_raw);

  (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN);
  SCD41_BusDeInit();

  if (status != SCD41_STATUS_OK)
  {
    return status;
  }

  *temperature = SCD41_ConvertTemperature(temperature_raw);
  *humidity = SCD41_ConvertHumidity(humidity_raw);

  return SCD41_STATUS_OK;
}

/* Power-cycled single shot, phase 1 of 3.
 * Wakes the sensor and starts the first, throw-away single shot. The ~5 s
 * measurement runs while the MCU is in low-power sleep; the result is discarded
 * in phase 2. Returns immediately - does not block on the measurement. */
int32_t SCD41_StartCo2SingleShot(void)
{
  int32_t status;

  if (SCD41_BusInit() != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  (void)SCD41_WriteCommand(SCD41_CMD_WAKE_UP);
  HAL_Delay(SCD41_WAKEUP_DELAY_MS);

  if (SCD41_AMBIENT_PRESSURE_MBAR > 0U)
  {
    status = SCD41_WriteCommandWithWord(SCD41_CMD_SET_AMBIENT_PRESSURE, (uint16_t)SCD41_AMBIENT_PRESSURE_MBAR);
    if (status != SCD41_STATUS_OK)
    {
      (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN);
      SCD41_BusDeInit();
      return status;
    }
  }

  status = SCD41_WriteCommand(SCD41_CMD_MEASURE_SINGLE_SHOT);
  if (status != SCD41_STATUS_OK) { (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN); }
  SCD41_BusDeInit();

  return status;
}

/* Power-cycled single shot, phase 2 of 3.
 * Called after the first (stabilisation) single shot has completed during
 * low-power sleep. Reads and discards that unstabilised result (Sensirion SCD4x
 * Low Power Operation, section 2.4), then starts the second, useful single shot.
 * That measurement runs during the next sleep window and is read by
 * SCD41_ReadCo2SingleShot() at the uplink. Does not block on a measurement: the
 * discarded shot is expected to be ready already, so the data-ready wait returns
 * promptly. */
int32_t SCD41_DiscardAndRestartCo2SingleShot(void)
{
  uint16_t discard_co2 = 0U;
  uint16_t discard_temperature = 0U;
  uint16_t discard_humidity = 0U;
  int32_t status;

  if (SCD41_BusInit() != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  /* The first single shot should already be finished; this returns promptly. */
  if (SCD41_WaitDataReady(SCD41_DATA_READY_TIMEOUT_MS) == SCD41_STATUS_OK)
  {
    (void)SCD41_ReadMeasurementWords(&discard_co2, &discard_temperature, &discard_humidity);
  }

  status = SCD41_WriteCommand(SCD41_CMD_MEASURE_SINGLE_SHOT);
  if (status != SCD41_STATUS_OK) { (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN); }
  SCD41_BusDeInit();

  return status;
}

int32_t SCD41_ReadCo2SingleShot(uint16_t *co2_ppm, int16_t *temperature, uint16_t *humidity)
{
  uint16_t co2_raw = 0;
  uint16_t temperature_raw = 0;
  uint16_t humidity_raw = 0;
  int32_t status;

  if (co2_ppm == NULL)
  {
    return SCD41_STATUS_ERROR;
  }

  if (SCD41_BusInit() != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  /* Power-cycled single shot, phase 3 of 3.
   * No WAKE_UP needed: sensor is already measuring the second (useful) single
   * shot started in SCD41_DiscardAndRestartCo2SingleShot(). SCD41 ignores all
   * commands except get_data_ready_status and read_measurement while a
   * measurement is in progress. */

  status = SCD41_WaitDataReady(SCD41_DATA_READY_TIMEOUT_MS);
  if (status != SCD41_STATUS_OK)
  {
    (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN);
    SCD41_BusDeInit();
    return status;
  }

  status = SCD41_ReadMeasurementWords(&co2_raw, &temperature_raw, &humidity_raw);

  (void)SCD41_WriteCommand(SCD41_CMD_POWER_DOWN);
  SCD41_BusDeInit();

  if (status != SCD41_STATUS_OK)
  {
    return status;
  }

  *co2_ppm = co2_raw;
  if (temperature != NULL)
  {
    *temperature = SCD41_ConvertTemperature(temperature_raw);
  }
  if (humidity != NULL)
  {
    *humidity = SCD41_ConvertHumidity(humidity_raw);
  }

  return SCD41_STATUS_OK;
}

int32_t SCD41_Sleep(void)
{
  if (SCD41_BusInit() != SCD41_STATUS_OK) { return SCD41_STATUS_ERROR; }
  return SCD41_WriteCommand(SCD41_CMD_POWER_DOWN);
}
