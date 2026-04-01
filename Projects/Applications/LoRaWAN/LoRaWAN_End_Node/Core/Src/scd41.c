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
#define SCD41_CMD_MEASURE_SINGLE_SHOT          0x219DU
#define SCD41_CMD_MEASURE_SINGLE_SHOT_RHT      0x2196U
#define SCD41_CMD_POWER_DOWN                   0x36E0U
#define SCD41_CMD_WAKE_UP                      0x36F6U

/* Private variables ---------------------------------------------------------*/
static I2C_HandleTypeDef hscd41_i2c;

/* Private function prototypes -----------------------------------------------*/
static uint8_t SCD41_CalculateCrc(const uint8_t *data, uint8_t length);
static int32_t SCD41_BusInit(void);
static void SCD41_BusDeInit(void);
static void SCD41_EnableGpioClock(GPIO_TypeDef *port);
static int32_t SCD41_WriteCommand(uint16_t command);
static int32_t SCD41_WriteCommandWithWord(uint16_t command, uint16_t data);
static int32_t SCD41_ReadMeasurementWords(uint16_t *co2_raw, uint16_t *temperature_raw, uint16_t *humidity_raw);
static float SCD41_ConvertTemperature(uint16_t temperature_raw);
static float SCD41_ConvertHumidity(uint16_t humidity_raw);

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

static void SCD41_EnableGpioClock(GPIO_TypeDef *port)
{
  if (port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else if (port == GPIOC)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  }
  else if (port == GPIOH)
  {
    __HAL_RCC_GPIOH_CLK_ENABLE();
  }
  else
  {
    /* Not supported in this project */
  }
}

static int32_t SCD41_BusInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  SCD41_EnableGpioClock(SCD41_I2C_SCL_GPIO_PORT);
  SCD41_EnableGpioClock(SCD41_I2C_SDA_GPIO_PORT);

  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = SCD41_I2C_GPIO_AF;

  GPIO_InitStruct.Pin = SCD41_I2C_SCL_PIN;
  HAL_GPIO_Init(SCD41_I2C_SCL_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SCD41_I2C_SDA_PIN;
  HAL_GPIO_Init(SCD41_I2C_SDA_GPIO_PORT, &GPIO_InitStruct);

  if (SCD41_I2C_INSTANCE == I2C2)
  {
    __HAL_RCC_I2C2_CLK_ENABLE();
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
  }
  else if (SCD41_I2C_INSTANCE == I2C1)
  {
    __HAL_RCC_I2C1_CLK_ENABLE();
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  }
  else
  {
    return SCD41_STATUS_ERROR;
  }

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  hscd41_i2c.Instance = SCD41_I2C_INSTANCE;
  hscd41_i2c.Init.Timing = SCD41_I2C_TIMING;
  hscd41_i2c.Init.OwnAddress1 = 0;
  hscd41_i2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hscd41_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hscd41_i2c.Init.OwnAddress2 = 0;
  hscd41_i2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hscd41_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hscd41_i2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hscd41_i2c) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hscd41_i2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hscd41_i2c, 0) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  return SCD41_STATUS_OK;
}

static void SCD41_BusDeInit(void)
{
  HAL_I2C_DeInit(&hscd41_i2c);

  if (SCD41_I2C_INSTANCE == I2C2)
  {
    __HAL_RCC_I2C2_CLK_DISABLE();
  }
  else if (SCD41_I2C_INSTANCE == I2C1)
  {
    __HAL_RCC_I2C1_CLK_DISABLE();
  }

  HAL_GPIO_DeInit(SCD41_I2C_SCL_GPIO_PORT, SCD41_I2C_SCL_PIN);
  HAL_GPIO_DeInit(SCD41_I2C_SDA_GPIO_PORT, SCD41_I2C_SDA_PIN);
}

static int32_t SCD41_WriteCommand(uint16_t command)
{
  uint8_t tx[2];

  tx[0] = (uint8_t)(command >> 8);
  tx[1] = (uint8_t)(command & 0xFFU);

  if (HAL_I2C_Master_Transmit(&hscd41_i2c, SCD41_I2C_ADDR_8BIT, tx, sizeof(tx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
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

  if (HAL_I2C_Master_Transmit(&hscd41_i2c, SCD41_I2C_ADDR_8BIT, tx, sizeof(tx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  return SCD41_STATUS_OK;
}

static int32_t SCD41_ReadMeasurementWords(uint16_t *co2_raw, uint16_t *temperature_raw, uint16_t *humidity_raw)
{
  uint8_t rx[9];

  if (SCD41_WriteCommand(SCD41_CMD_READ_MEASUREMENT) != SCD41_STATUS_OK)
  {
    return SCD41_STATUS_ERROR;
  }

  HAL_Delay(1U);

  if (HAL_I2C_Master_Receive(&hscd41_i2c, SCD41_I2C_ADDR_8BIT, rx, sizeof(rx), SCD41_I2C_TIMEOUT_MS) != HAL_OK)
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

static float SCD41_ConvertTemperature(uint16_t temperature_raw)
{
  return (-45.0f + (175.0f * ((float)temperature_raw / 65536.0f)));
}

static float SCD41_ConvertHumidity(uint16_t humidity_raw)
{
  return (100.0f * ((float)humidity_raw / 65536.0f));
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

int32_t SCD41_ReadRhtSingleShot(float *temperature, float *humidity)
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
      SCD41_BusDeInit();
      return status;
    }
  }

  status = SCD41_WriteCommand(SCD41_CMD_MEASURE_SINGLE_SHOT);
  SCD41_BusDeInit();

  return status;
}

int32_t SCD41_ReadCo2SingleShot(uint16_t *co2_ppm, float *temperature, float *humidity)
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

  (void)SCD41_WriteCommand(SCD41_CMD_WAKE_UP);
  HAL_Delay(SCD41_WAKEUP_DELAY_MS);

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
