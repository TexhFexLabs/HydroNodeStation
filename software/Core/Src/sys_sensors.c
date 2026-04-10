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
#include "lc709203f.h"

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
static uint8_t SCD41Available = 0U;
static uint8_t LC709203FAvailable = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
int32_t EnvSensors_ReadRhtSingleShot(float *temperature, float *humidity)
{
  /* USER CODE BEGIN EnvSensors_ReadRhtSingleShot */
#if (SCD41_ENABLED == 1)
  if ((temperature == NULL) || (humidity == NULL) || (SCD41Available == 0U))
  {
    return -1;
  }

  return (SCD41_ReadRhtSingleShot(temperature, humidity) == SCD41_STATUS_OK) ? 0 : -1;
#else
  (void)temperature;
  (void)humidity;
  return -1;
#endif
  /* USER CODE END EnvSensors_ReadRhtSingleShot */
}

int32_t EnvSensors_StartCo2SingleShot(void)
{
  /* USER CODE BEGIN EnvSensors_StartCo2SingleShot */
#if (SCD41_ENABLED == 1)
  if (SCD41Available == 0U)
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
  if ((sensor_data == NULL) || (SCD41Available == 0U))
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

int32_t EnvSensors_ReadBatteryVoltageMv(uint16_t *voltage_mv)
{
  /* USER CODE BEGIN EnvSensors_ReadBatteryVoltageMv */
#if (LC709203F_ENABLED == 1)
  if ((voltage_mv == NULL) || (LC709203FAvailable == 0U))
  {
    return -1;
  }

  return (LC709203F_ReadVoltageMv(voltage_mv) == LC709203F_STATUS_OK) ? 0 : -1;
#else
  (void)voltage_mv;
  return -1;
#endif
  /* USER CODE END EnvSensors_ReadBatteryVoltageMv */
}

int32_t EnvSensors_Init(void)
{
  /* USER CODE BEGIN EnvSensors_Init */
  int32_t init_status = 0;

#if (SCD41_ENABLED == 1)
  if (SCD41_Init() == SCD41_STATUS_OK)
  {
    SCD41Available = 1U;
    APP_LOG(TS_OFF, VLEVEL_M, "SCD41 init OK\r\n");
  }
  else
  {
    SCD41Available = 0U;
    APP_LOG(TS_OFF, VLEVEL_M, "SCD41 init failed, sensor data disabled\r\n");
    init_status = -1;
  }
#else
  SCD41Available = 0U;
#endif

#if (LC709203F_ENABLED == 1)
  if (LC709203F_Init() == LC709203F_STATUS_OK)
  {
    LC709203FAvailable = 1U;
    APP_LOG(TS_OFF, VLEVEL_M, "LC709203F init OK\r\n");
  }
  else
  {
    LC709203FAvailable = 0U;
    APP_LOG(TS_OFF, VLEVEL_M, "LC709203F init failed, battery voltage disabled\r\n");
    init_status = -1;
  }
#else
  LC709203FAvailable = 0U;
#endif

  return init_status;
  /* USER CODE END EnvSensors_Init */
}

/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Private Functions Definition -----------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
