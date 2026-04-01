/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.c
  * @author  MCD Application Team
  * @brief   Manages the sensors on the application
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "sys_conf.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */
#include "sys_app.h"
#include "scd41.h"

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
static uint8_t EnvSensorAvailable = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
int32_t EnvSensors_Read(sensor_t *sensor_data)
{
  /* USER CODE BEGIN EnvSensors_Read */
  if (sensor_data == NULL)
  {
    return -1;
  }

  sensor_data->temperature = 0.0f;
  sensor_data->humidity    = 0.0f;
  sensor_data->co2         = 0;

#if (SCD41_ENABLED == 1)
  if (EnvSensorAvailable != 0U)
  {
    if (SCD41_ReadRhtSingleShot(&sensor_data->temperature, &sensor_data->humidity) != SCD41_STATUS_OK)
    {
      return -1;
    }
  }
#endif

  return 0;
  /* USER CODE END EnvSensors_Read */
}

int32_t EnvSensors_StartCo2SingleShot(void)
{
  /* USER CODE BEGIN EnvSensors_StartCo2SingleShot */
#if (SCD41_ENABLED == 1)
  if (EnvSensorAvailable == 0U)
  {
    return -1;
  }
  return (SCD41_StartCo2SingleShot() == SCD41_STATUS_OK) ? 0 : -1;
#else
  return -1;
#endif
  /* USER CODE END EnvSensors_StartCo2SingleShot */
}

int32_t EnvSensors_ReadCo2SingleShot(sensor_t *sensor_data)
{
  /* USER CODE BEGIN EnvSensors_ReadCo2SingleShot */
#if (SCD41_ENABLED == 1)
  if ((sensor_data == NULL) || (EnvSensorAvailable == 0U))
  {
    return -1;
  }

  return (SCD41_ReadCo2SingleShot(&sensor_data->co2, &sensor_data->temperature, &sensor_data->humidity) == SCD41_STATUS_OK) ? 0 : -1;
#else
  (void)sensor_data;
  return -1;
#endif
  /* USER CODE END EnvSensors_ReadCo2SingleShot */
}

int32_t EnvSensors_Init(void)
{
  /* USER CODE BEGIN EnvSensors_Init */
#if (SCD41_ENABLED == 1)
  if (SCD41_Init() == SCD41_STATUS_OK)
  {
    EnvSensorAvailable = 1U;
    APP_LOG(TS_OFF, VLEVEL_M, "SCD41 init OK\r\n");
  }
  else
  {
    EnvSensorAvailable = 0U;
    APP_LOG(TS_OFF, VLEVEL_M, "SCD41 init failed, sensor data disabled\r\n");
    return -1;
  }
#else
  EnvSensorAvailable = 0U;
#endif

  return 0;
  /* USER CODE END EnvSensors_Init */
}

/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Private Functions Definition -----------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
