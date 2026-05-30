/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.c
  * @author  MCD Application Team
  * @brief   Manages the sensors on the application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "platform.h"
#include "sys_conf.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */
#include "sht45.h"
#include "bmp390.h"
#include "ltr390.h"
#include "max17048.h"
#include "scd41.h"
#include "sps30.h"
#include "i2c.h"
#include "sys_app.h"
#include "adc_if.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/

/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
int32_t EnvSensors_Read(sensor_t *sensor_data, uint8_t sensor_flags)
{
  /* USER CODE BEGIN EnvSensors_Read */
  if (sensor_data == NULL)
  {
    return -1;
  }

  /* Default values */
  sensor_data->humidity         = 0U;
  sensor_data->temperature      = 0;
  sensor_data->pressure         = 0U;
  sensor_data->battery_voltage  = 0U;
  sensor_data->uvi_x100         = 0U;
  sensor_data->co2_ppm          = 0U;
  sensor_data->pm1_0            = 0U;
  sensor_data->pm2_5            = 0U;
  sensor_data->pm4_0            = 0U;
  sensor_data->pm10_0           = 0U;
  sensor_data->nc_0_5           = 0U;
  sensor_data->nc_1_0           = 0U;
  sensor_data->nc_2_5           = 0U;
  sensor_data->nc_4_0           = 0U;
  sensor_data->nc_10_0          = 0U;
  sensor_data->typ_size         = 0U;

  /* 1. Read MAX17048 */
  MAX17048_Data_t max17048;
  if (MAX17048_Read(&max17048) == MAX17048_OK)
  {
    sensor_data->battery_voltage = max17048.voltage_mv;
  }
  else
  {
    /* Try to recover I2C and re-init fuel gauge when first read fails at boot. */
    I2C2_RecoverBus();
    if ((MAX17048_Init() == MAX17048_OK) && (MAX17048_Read(&max17048) == MAX17048_OK))
    {
      sensor_data->battery_voltage = max17048.voltage_mv;
    }
  }

  if (sensor_flags & SENSOR_FLAG_ONLY_BATTERY)
  {
    return 0;
  }

  /* 2. Read SHT45 */
  SHT45_Data_t sht45;
  if (SHT45_Read(&sht45) == SHT45_OK)
  {
    sensor_data->temperature = sht45.temperature;   /* 0.01 degC */
    sensor_data->humidity    = sht45.humidity;      /* 0.01 %RH  */
  }
  else
  {
    I2C2_RecoverBus();
  }

  /* 3. Read BMP390 */
  BMP390_Data_t bmp390;
  if (BMP390_Read(&bmp390) == BMP390_OK)
  {
    sensor_data->pressure = bmp390.pressure_hPa;  /* 0.1 hPa */
  }
  else
  {
    I2C2_RecoverBus();
  }

  /* 4. Read LTR390 */
  LTR390_Data_t ltr390;
  if (LTR390_ReadUV(&ltr390) == LTR390_OK)
  {
    sensor_data->uvi_x100 = ltr390.uvi_x100;
  }

  /* 5. Read SCD41 */
  if (sensor_flags & SENSOR_FLAG_CO2)
  {
    uint16_t scd41_co2_ppm = 0U;
    if (SCD41_ReadCo2SingleShot(&scd41_co2_ppm, NULL, NULL) == SCD41_STATUS_OK)
    {
      sensor_data->co2_ppm = scd41_co2_ppm;
    }
  }

  /* 6. Read SPS30 */
  if (sensor_flags & SENSOR_FLAG_SPS30)
  {
    SPS30_Data_t sps30_data;
    if (SPS30_ReadMeasurement(&sps30_data) == SPS30_STATUS_OK)
    {
      sensor_data->pm1_0    = sps30_data.mc_1_0;   /* 0.1 ug/m3 */
      sensor_data->pm2_5    = sps30_data.mc_2_5;
      sensor_data->pm4_0    = sps30_data.mc_4_0;
      sensor_data->pm10_0   = sps30_data.mc_10_0;
      sensor_data->nc_0_5   = sps30_data.nc_0_5;   /* 0.1 #/cm3 */
      sensor_data->nc_1_0   = sps30_data.nc_1_0;
      sensor_data->nc_2_5   = sps30_data.nc_2_5;
      sensor_data->nc_4_0   = sps30_data.nc_4_0;
      sensor_data->nc_10_0  = sps30_data.nc_10_0;
      sensor_data->typ_size = sps30_data.typ_size; /* nm */
    }
  }

  return 0;
  /* USER CODE END EnvSensors_Read */
}

int32_t EnvSensors_Init(void)
{
  /* USER CODE BEGIN EnvSensors_Init */
  if (SHT45_Init() != SHT45_OK)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "SHT45 not found\r\n");
  }

  if (BMP390_Init() != BMP390_OK)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "BMP390 not found\r\n");
  }

  if (LTR390_Init() != LTR390_OK)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "LTR390 not found\r\n");
  }

  if (MAX17048_Init() != MAX17048_OK)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "MAX17048 not found\r\n");
  }

  if (SCD41_Init() != SCD41_STATUS_OK)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "SCD41 not found\r\n");
  }

  if (SPS30_Init() != SPS30_STATUS_OK)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "SPS30 not found\r\n");
  }

  return 0;
  /* USER CODE END EnvSensors_Init */
}

/* USER CODE BEGIN EF */
int32_t EnvSensors_StartPreMeasurement(uint8_t sensor_flags)
{
  if(sensor_flags & SENSOR_FLAG_CO2)
  {
    return SCD41_StartCo2SingleShot();
  }
  if(sensor_flags & SENSOR_FLAG_SPS30)
  {
    if (SPS30_WakeUp() == SPS30_STATUS_OK)
    {
      return SPS30_StartMeasurement();
    }
    return SPS30_STATUS_ERROR;
  }
  return -1;
}
/* USER CODE END EF */

/* Private Functions Definition -----------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

