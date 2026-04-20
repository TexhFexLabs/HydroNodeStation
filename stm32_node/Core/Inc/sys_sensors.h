/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.h
  * @author  MCD Application Team
  * @brief   Header for sensors application
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SENSORS_H__
#define __SENSORS_H__

#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/**
  * Sensor data parameters
  */
typedef struct
{
  float pressure;         /*!< in mbar */
  float temperature;      /*!< in degC */
  float humidity;         /*!< in % */
  uint16_t battery_voltage;  /*!< battery voltage in V */
  uint32_t uv_raw;        /*!< LTR390 UV raw counts */
  uint16_t co2_ppm;       /*!< SCD41 CO2 in ppm */
  float pm1_0;            /*!< SPS30 PM1.0 in ug/m3 */
  float pm2_5;            /*!< SPS30 PM2.5 in ug/m3 */
  float pm4_0;            /*!< SPS30 PM4.0 in ug/m3 */
  float pm10_0;           /*!< SPS30 PM10.0 in ug/m3 */
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

/* USER CODE BEGIN EFP */
/* USER CODE BEGIN sensor_flags */
#define SENSOR_FLAG_CO2   (1U << 0)         /*!< Read SCD41 CO2 this cycle */
#define SENSOR_FLAG_SPS30 (1U << 1)         /*!< Read SPS30 particulates this cycle */
#define SENSOR_FLAG_ONLY_BATTERY (1U << 2)  /*!< Read only battery voltage */
/* USER CODE END sensor_flags */

/**
  * @brief  Environmental sensor read.
  */
int32_t EnvSensors_Read(sensor_t *sensor_data, uint8_t sensor_flags);

/**
  * @brief  Start pre-measurement
  */
int32_t EnvSensors_StartPreMeasurement(uint8_t sensor_flags);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H__ */
