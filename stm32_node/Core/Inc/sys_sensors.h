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
  uint16_t pressure;      /*!< pressure in 0.1 hPa (hPa*10) */
  int16_t  temperature;   /*!< temperature in 0.01 degC (degC*100) */
  uint16_t humidity;      /*!< humidity in 0.01 % (%*100) */
  uint16_t battery_voltage;  /*!< battery voltage in mV */
  uint16_t uvi_x100;      /*!< LTR390 UV Index * 100 (e.g. 350 = UVI 3.50) */
  uint16_t co2_ppm;       /*!< SCD41 CO2 in ppm */
  uint16_t pm1_0;         /*!< SPS30 PM1.0 MC in 0.1 ug/m3 */
  uint16_t pm2_5;         /*!< SPS30 PM2.5 MC in 0.1 ug/m3 */
  uint16_t pm4_0;         /*!< SPS30 PM4.0 MC in 0.1 ug/m3 */
  uint16_t pm10_0;        /*!< SPS30 PM10.0 MC in 0.1 ug/m3 */
  uint16_t nc_0_5;        /*!< SPS30 PM0.5 NC in 0.1 #/cm3 */
  uint16_t nc_1_0;        /*!< SPS30 PM1.0 NC in 0.1 #/cm3 */
  uint16_t nc_2_5;        /*!< SPS30 PM2.5 NC in 0.1 #/cm3 */
  uint16_t nc_4_0;        /*!< SPS30 PM4.0 NC in 0.1 #/cm3 */
  uint16_t nc_10_0;       /*!< SPS30 PM10  NC in 0.1 #/cm3 */
  uint16_t typ_size;      /*!< SPS30 Typical Particle Size in nm (um*1000) */
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
