/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lc709203f.c
  * @author  MCD Application Team
  * @brief   LC709203F battery monitor low-level driver
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_conf.h"
#include "i2c.h"
#include "lc709203f.h"

/* Private define ------------------------------------------------------------*/
#define LC709203F_I2C_ADDR_8BIT               (0x0BU << 1)

#define LC709203F_CMD_CELLVOLTAGE             0x09U
#define LC709203F_CMD_ICVERSION               0x11U
#define LC709203F_CMD_POWERMODE               0x15U

#define LC709203F_POWERMODE_OPERATE           0x0001U
#define LC709203F_POWERMODE_SLEEP             0x0002U

/* Private function prototypes -----------------------------------------------*/
static uint8_t LC709203F_CalculatePec(const uint8_t *data, uint8_t length);
static int32_t LC709203F_BusInit(void);
static void LC709203F_BusDeInit(void);
static int32_t LC709203F_WriteWord(uint8_t command, uint16_t data);
static int32_t LC709203F_ReadWord(uint8_t command, uint16_t *data);
static int32_t LC709203F_SetPowerMode(uint16_t mode);

/* Private functions ---------------------------------------------------------*/
static uint8_t LC709203F_CalculatePec(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0x00U;
  uint8_t i;
  uint8_t bit;

  for (i = 0U; i < length; i++)
  {
    crc ^= data[i];
    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1U) ^ 0x07U);
      }
      else
      {
        crc <<= 1U;
      }
    }
  }

  return crc;
}

static int32_t LC709203F_BusInit(void)
{
  if ((hi2c2.Instance != I2C2) || (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_RESET))
  {
    MX_I2C2_Init();
  }

  if (hi2c2.Instance != I2C2)
  {
    return LC709203F_STATUS_ERROR;
  }

  return LC709203F_STATUS_OK;
}

static void LC709203F_BusDeInit(void)
{
  (void)HAL_I2C_DeInit(&hi2c2);
}

static int32_t LC709203F_WriteWord(uint8_t command, uint16_t data)
{
  uint8_t tx[4];
  uint8_t pec_data[4];

  tx[0] = command;
  tx[1] = (uint8_t)(data & 0x00FFU);
  tx[2] = (uint8_t)(data >> 8);

  pec_data[0] = LC709203F_I2C_ADDR_8BIT;
  pec_data[1] = command;
  pec_data[2] = tx[1];
  pec_data[3] = tx[2];
  tx[3] = LC709203F_CalculatePec(pec_data, 4U);

  if (HAL_I2C_Master_Transmit(&hi2c2, LC709203F_I2C_ADDR_8BIT, tx, sizeof(tx), LC709203F_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return LC709203F_STATUS_ERROR;
  }

  return LC709203F_STATUS_OK;
}

static int32_t LC709203F_ReadWord(uint8_t command, uint16_t *data)
{
  uint8_t tx = command;
  uint8_t rx[3];
  uint8_t pec_data[5];

  if (data == NULL)
  {
    return LC709203F_STATUS_ERROR;
  }

  if (HAL_I2C_Master_Transmit(&hi2c2, LC709203F_I2C_ADDR_8BIT, &tx, sizeof(tx), LC709203F_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return LC709203F_STATUS_ERROR;
  }

  if (HAL_I2C_Master_Receive(&hi2c2, LC709203F_I2C_ADDR_8BIT, rx, sizeof(rx), LC709203F_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return LC709203F_STATUS_ERROR;
  }

  pec_data[0] = LC709203F_I2C_ADDR_8BIT;
  pec_data[1] = command;
  pec_data[2] = (uint8_t)(LC709203F_I2C_ADDR_8BIT | 0x01U);
  pec_data[3] = rx[0];
  pec_data[4] = rx[1];

  if (LC709203F_CalculatePec(pec_data, 5U) != rx[2])
  {
    return LC709203F_STATUS_CRC_ERROR;
  }

  *data = (uint16_t)(((uint16_t)rx[1] << 8) | rx[0]);

  return LC709203F_STATUS_OK;
}

static int32_t LC709203F_SetPowerMode(uint16_t mode)
{
  return LC709203F_WriteWord(LC709203F_CMD_POWERMODE, mode);
}

/* Exported functions --------------------------------------------------------*/
int32_t LC709203F_Init(void)
{
  int32_t status;

  if (LC709203F_BusInit() != LC709203F_STATUS_OK)
  {
    return LC709203F_STATUS_ERROR;
  }

  status = LC709203F_SetPowerMode(LC709203F_POWERMODE_OPERATE);
  if (status != LC709203F_STATUS_OK)
  {
    LC709203F_BusDeInit();
    return status;
  }

  HAL_Delay(LC709203F_WAKEUP_DELAY_MS);

  (void)LC709203F_SetPowerMode(LC709203F_POWERMODE_SLEEP);
  LC709203F_BusDeInit();

  return status;
}

int32_t LC709203F_ReadVoltageMv(uint16_t *voltage_mv)
{
  uint16_t voltage_raw = 0U;
  int32_t status;

  if (voltage_mv == NULL)
  {
    return LC709203F_STATUS_ERROR;
  }

  if (LC709203F_BusInit() != LC709203F_STATUS_OK)
  {
    return LC709203F_STATUS_ERROR;
  }

  status = LC709203F_SetPowerMode(LC709203F_POWERMODE_OPERATE);
  if (status != LC709203F_STATUS_OK)
  {
    LC709203F_BusDeInit();
    return status;
  }

  HAL_Delay(LC709203F_WAKEUP_DELAY_MS);

  status = LC709203F_ReadWord(LC709203F_CMD_CELLVOLTAGE, &voltage_raw);

  (void)LC709203F_SetPowerMode(LC709203F_POWERMODE_SLEEP);
  LC709203F_BusDeInit();

  if (status != LC709203F_STATUS_OK)
  {
    return status;
  }

  if (voltage_raw < LC709203F_VOLTAGE_MIN_MV)
  {
    voltage_raw = LC709203F_VOLTAGE_MIN_MV;
  }
  else if (voltage_raw > LC709203F_VOLTAGE_MAX_MV)
  {
    voltage_raw = LC709203F_VOLTAGE_MAX_MV;
  }

  *voltage_mv = voltage_raw;

  return LC709203F_STATUS_OK;
}
