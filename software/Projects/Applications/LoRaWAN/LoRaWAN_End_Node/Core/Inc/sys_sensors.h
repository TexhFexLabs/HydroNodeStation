/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.h
  * @author  MCD Application Team
  * @brief   Header for sensors application
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SENSORS_H__
#define __SENSORS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/**
  * Sensor data parameters
  */
typedef struct
{
  float temperature;      /*!< in degC */
  float humidity;         /*!< in % */
  uint16_t co2;           /*!< in ppm */
  /* USER CODE BEGIN sensor_t */

  /* USER CODE END sensor_t */
} sensor_t;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  initialize the environmental sensor
  */
int32_t EnvSensors_Init(void);

/**
  * @brief  read SCD41 single-shot temperature and humidity
  */
int32_t EnvSensors_ReadRhtSingleShot(float *temperature, float *humidity);

/**
  * @brief  start SCD41 single-shot measurement (CO2 + RH + T)
  */
int32_t EnvSensors_StartCo2SingleShot(void);

/**
  * @brief  read result of SCD41 single-shot measurement
  * @param  sensor_data pointer to sensor data struct
  */
int32_t EnvSensors_ReadCo2SingleShot(sensor_t *sensor_data);

/**
  * @brief  read battery voltage from LC709203F in mV
  */
int32_t EnvSensors_ReadBatteryVoltageMv(uint16_t *voltage_mv);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H__ */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
