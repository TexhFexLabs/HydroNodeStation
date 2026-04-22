/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sps30.c
  * @author  Gemini CLI
  * @brief   SPS30 particulate matter sensor low-level driver
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_conf.h"
#include "i2c.h"
#include "sps30.h"
#include <string.h>

/* Private define ------------------------------------------------------------*/
#define SPS30_CMD_START_MEASUREMENT     0x0010U
#define SPS30_CMD_STOP_MEASUREMENT      0x0104U
#define SPS30_CMD_READ_DATA_READY_FLAG  0x0202U
#define SPS30_CMD_READ_MEASURED_VALUES  0x0300U
#define SPS30_CMD_SLEEP                 0x1001U
#define SPS30_CMD_WAKE_UP                0x1103U
#define SPS30_CMD_FAN_CLEAN_INTERVAL    0x8004U
#define SPS30_CMD_START_FAN_CLEANING    0x5607U
#define SPS30_CMD_RESET                 0xD304U

/* Private function prototypes -----------------------------------------------*/
static uint8_t SPS30_CalculateCrc(const uint8_t *data, uint8_t length);
static int32_t SPS30_BusInit(void);
static void SPS30_BusDeInit(void);
static int32_t SPS30_EnsureBusReady(void);
static int32_t SPS30_WriteCommand(uint16_t command);
static int32_t SPS30_WriteCommandWithData(uint16_t command, const uint8_t *data, uint8_t length);

/* Private variables ---------------------------------------------------------*/
static uint8_t sps30_bus_acquire_count = 0U;

/* Private functions ---------------------------------------------------------*/
static uint8_t SPS30_CalculateCrc(const uint8_t *data, uint8_t length)
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

static int32_t SPS30_BusInit(void)
{
  if ((hi2c2.Instance != I2C2) || (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_RESET))
  {
    MX_I2C2_Init();
  }

  if (hi2c2.Instance != I2C2)
  {
    return SPS30_STATUS_ERROR;
  }

  return SPS30_STATUS_OK;
}

static void SPS30_BusDeInit(void)
{
  (void)HAL_I2C_DeInit(&hi2c2);
}

static int32_t SPS30_EnsureBusReady(void)
{
  if (sps30_bus_acquire_count > 0U)
  {
    return SPS30_STATUS_OK;
  }

  return SPS30_BusInit();
}

static int32_t SPS30_WriteCommand(uint16_t command)
{
  uint8_t tx[2];

  tx[0] = (uint8_t)(command >> 8);
  tx[1] = (uint8_t)(command & 0xFFU);

  if (HAL_I2C_Master_Transmit(&hi2c2, SPS30_I2C_ADDR_8BIT, tx, sizeof(tx), SPS30_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  return SPS30_STATUS_OK;
}

static int32_t SPS30_WriteCommandWithData(uint16_t command, const uint8_t *data, uint8_t length)
{
  uint8_t tx[2 + 3 * (length / 2)]; // 2 bytes command + (2 bytes data + 1 byte CRC) per word
  uint8_t i, j = 0;

  tx[j++] = (uint8_t)(command >> 8);
  tx[j++] = (uint8_t)(command & 0xFFU);

  for (i = 0; i < length; i += 2)
  {
    tx[j++] = data[i];
    tx[j++] = data[i + 1];
    tx[j++] = SPS30_CalculateCrc(&data[i], 2);
  }

  if (HAL_I2C_Master_Transmit(&hi2c2, SPS30_I2C_ADDR_8BIT, tx, j, SPS30_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  return SPS30_STATUS_OK;
}

/* Exported functions --------------------------------------------------------*/
int32_t SPS30_Init(void)
{
  if (SPS30_BusInit() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  /* Wake up first to be able to send commands */
  (void)SPS30_WakeUp();

  /* Disable auto cleaning interval (set to 0) */
  (void)SPS30_SetFanAutoCleaningInterval(0U);

  /* Ensure it's in sleep mode for low power */
  (void)SPS30_Sleep();
  
  SPS30_BusDeInit();
  return SPS30_STATUS_OK;
}

int32_t SPS30_AcquireBus(void)
{
  if (sps30_bus_acquire_count == 0U)
  {
    if (SPS30_BusInit() != SPS30_STATUS_OK)
    {
      return SPS30_STATUS_ERROR;
    }
  }

  sps30_bus_acquire_count++;
  return SPS30_STATUS_OK;
}

int32_t SPS30_ReleaseBus(void)
{
  if (sps30_bus_acquire_count > 0U)
  {
    sps30_bus_acquire_count--;
  }

  return SPS30_STATUS_OK;
}

int32_t SPS30_WakeUp(void)
{
  if (SPS30_EnsureBusReady() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  /* Two wake-up commands or I2C start-stop */
  /* According to datasheet 6.3.6: send 0x1103 twice to activate interface */
  (void)SPS30_WriteCommand(SPS30_CMD_WAKE_UP);
  (void)SPS30_WriteCommand(SPS30_CMD_WAKE_UP);
  
  HAL_Delay(SPS30_WAKEUP_DELAY_MS);
  
  return SPS30_STATUS_OK;
}

int32_t SPS30_StartMeasurement(void)
{
  uint8_t data[2];
  data[0] = 0x03; // Big-endian IEEE754 float values
  data[1] = 0x00; // dummy byte

  if (SPS30_EnsureBusReady() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  int32_t status = SPS30_WriteCommandWithData(SPS30_CMD_START_MEASUREMENT, data, 2);
  HAL_Delay(SPS30_START_MEASUREMENT_DELAY_MS);
  return status;
}

int32_t SPS30_ReadMeasurement(SPS30_Data_t *data)
{
  if (data == NULL) return SPS30_STATUS_ERROR;

  uint8_t rx[60]; /* 10 IEEE754 floats * (2 bytes + 1 CRC) = 60 bytes */
  uint8_t i, j;

  if (SPS30_EnsureBusReady() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  if (SPS30_WriteCommand(SPS30_CMD_READ_MEASURED_VALUES) != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  if (HAL_I2C_Master_Receive(&hi2c2, SPS30_I2C_ADDR_8BIT, rx, 60, SPS30_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  /* Verify CRC for all 10 float words (we only keep the first 4). */
  for (i = 0, j = 0; i < 10; i++, j += 6)
  {
    uint8_t word1[2] = { rx[j], rx[j + 1] };
    uint8_t word2[2] = { rx[j + 3], rx[j + 4] };
    if (SPS30_CalculateCrc(word1, 2) != rx[j + 2] ||
        SPS30_CalculateCrc(word2, 2) != rx[j + 5])
    {
      return SPS30_STATUS_CRC_ERROR;
    }
  }

  /* Decode all 10 big-endian IEEE754 floats and convert to scaled uint16.
   * Float is local/scratch only; no float stored in SPS30_Data_t.
   * Index 0..3 : MC   -> *10  (0.1 ug/m3)
   * Index 4..8 : NC   -> *10  (0.1 #/cm3)
   * Index 9    : Typ size -> *1000 (nm) */
  uint16_t *out[10] = {
    &data->mc_1_0, &data->mc_2_5, &data->mc_4_0, &data->mc_10_0,
    &data->nc_0_5, &data->nc_1_0, &data->nc_2_5, &data->nc_4_0, &data->nc_10_0,
    &data->typ_size
  };
  for (i = 0, j = 0; i < 10; i++, j += 6)
  {
    uint32_t raw_val = ((uint32_t)rx[j]     << 24) |
                       ((uint32_t)rx[j + 1] << 16) |
                       ((uint32_t)rx[j + 3] <<  8) |
                       ((uint32_t)rx[j + 4]);
    float f;
    memcpy(&f, &raw_val, 4);

    float scale = (i < 9) ? 10.0f : 1000.0f;
    float v = f * scale;
    if (v < 0.0f)     { v = 0.0f; }
    if (v > 65535.0f) { v = 65535.0f; }
    *out[i] = (uint16_t)(v + 0.5f);
  }

  return SPS30_STATUS_OK;
}

int32_t SPS30_StopMeasurement(void)
{
  if (SPS30_EnsureBusReady() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  return SPS30_WriteCommand(SPS30_CMD_STOP_MEASUREMENT);
}

int32_t SPS30_SetFanAutoCleaningInterval(uint32_t interval_s)
{
  uint8_t data[4];
  data[0] = (uint8_t)((interval_s >> 24) & 0xFFU);
  data[1] = (uint8_t)((interval_s >> 16) & 0xFFU);
  data[2] = (uint8_t)((interval_s >> 8) & 0xFFU);
  data[3] = (uint8_t)(interval_s & 0xFFU);

  return SPS30_WriteCommandWithData(SPS30_CMD_FAN_CLEAN_INTERVAL, data, 4);
}

int32_t SPS30_StartFanCleaning(void)
{
  if (SPS30_EnsureBusReady() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  int32_t status = SPS30_WriteCommand(SPS30_CMD_START_FAN_CLEANING);
  HAL_Delay(20);
  return status;
}

int32_t SPS30_Sleep(void)
{
  if (SPS30_EnsureBusReady() != SPS30_STATUS_OK)
  {
    return SPS30_STATUS_ERROR;
  }

  int32_t status = SPS30_WriteCommand(SPS30_CMD_SLEEP);
  return status;
}
