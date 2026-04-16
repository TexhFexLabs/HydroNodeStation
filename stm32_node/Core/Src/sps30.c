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
#define SPS30_CMD_RESET                 0xD304U

/* Private function prototypes -----------------------------------------------*/
static uint8_t SPS30_CalculateCrc(const uint8_t *data, uint8_t length);
static int32_t SPS30_BusInit(void);
static void SPS30_BusDeInit(void);
static int32_t SPS30_WriteCommand(uint16_t command);
static int32_t SPS30_WriteCommandWithData(uint16_t command, const uint8_t *data, uint8_t length);

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

  /* Just a simple ping/check or reset */
  /* SPS30 enters Idle-Mode after power up */
  /* Ensure it's in sleep mode for low power if not needed now */
  SPS30_Sleep();
  
  SPS30_BusDeInit();
  return SPS30_STATUS_OK;
}

int32_t SPS30_WakeUp(void)
{
  if (SPS30_BusInit() != SPS30_STATUS_OK)
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

  /* No BusInit here, assuming it's called after WakeUp */
  int32_t status = SPS30_WriteCommandWithData(SPS30_CMD_START_MEASUREMENT, data, 2);
  HAL_Delay(SPS30_START_MEASUREMENT_DELAY_MS);
  return status;
}

int32_t SPS30_ReadMeasurement(SPS30_Data_t *data)
{
  if (data == NULL) return SPS30_STATUS_ERROR;

  uint8_t rx[60]; // 10 floats * (2 bytes + 1 byte CRC) = 60 bytes
  uint8_t i, j;
  float *float_ptr = (float *)data;

  if (SPS30_BusInit() != SPS30_STATUS_OK)
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

  for (i = 0, j = 0; i < 10; i++)
  {
    uint8_t word1[2] = { rx[j], rx[j + 1] };
    uint8_t crc1 = rx[j + 2];
    uint8_t word2[2] = { rx[j + 3], rx[j + 4] };
    uint8_t crc2 = rx[j + 5];

    if (SPS30_CalculateCrc(word1, 2) != crc1 || SPS30_CalculateCrc(word2, 2) != crc2)
    {
      return SPS30_STATUS_CRC_ERROR;
    }

    /* Convert to float (Big-endian) */
    uint32_t raw_val = ((uint32_t)rx[j] << 24) | ((uint32_t)rx[j + 1] << 16) | 
                       ((uint32_t)rx[j + 3] << 8) | (uint32_t)rx[j + 4];
    memcpy(&float_ptr[i], &raw_val, 4);
    
    j += 6;
  }

  return SPS30_STATUS_OK;
}

int32_t SPS30_StopMeasurement(void)
{
  return SPS30_WriteCommand(SPS30_CMD_STOP_MEASUREMENT);
}

int32_t SPS30_Sleep(void)
{
  int32_t status = SPS30_WriteCommand(SPS30_CMD_SLEEP);
  return status;
}
