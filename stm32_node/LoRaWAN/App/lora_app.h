/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.h
  * @author  MCD Application Team
  * @brief   Header of application of the LRWAN Middleware
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021-2025 STMicroelectronics.
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
#ifndef __LORA_APP_H__
#define __LORA_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/

/* LoraWAN application configuration (Mw is configured by lorawan_conf.h) */
#define ACTIVE_REGION                               LORAMAC_REGION_EU868

/* USER CODE BEGIN EC_CAYENNE_LPP */
/*!
 * Low battery threshold in milli volt
 */
#define LOW_BATTERY_THRESHOLD_MV                    3400
/*!
 * ULTRA low battery threshold in milli volt
 */
#define ULTRA_LOW_BATTERY_THRESHOLD_MV                    3000
/*!
  * Defines the application data transmission duty cycle in seconds.
  * @note This is the default/fallback value. The active value is configurable at
  *       runtime via downlink and persisted in flash (see lora_app.c).
  */
#define APP_TX_DUTYCYCLE                            180

/*!
  * Min/max bounds (seconds) for the runtime-configurable TX duty cycle.
  * @note Floor stays well above the SPS30 pre-measurement time (16.5 s).
  */
#define APP_TX_DUTYCYCLE_MIN_S                      30U
#define APP_TX_DUTYCYCLE_MAX_S                      3600U

/**
  * @brief SCD41 pre-measurement time in milliseconds (power-cycled single shot, phase 1).
  * @note Time before the uplink at which the node wakes to start the first,
  *       throw-away stabilisation single shot. That measurement (~5 s) runs while the
  *       MCU is asleep; the MCU never busy-waits on it. Phase 2 follows at
  *       SCD41_RESTART_TIME_MS. Must leave room for both single shots plus margin.
  */
#define SCD41_PRE_MEASUREMENT_TIME_MS               12000

/**
  * @brief SCD41 restart time in milliseconds (power-cycled single shot, phase 2).
  * @note Time before the uplink at which the node wakes to discard the stabilisation
  *       shot and start the second, useful single shot. Must be < SCD41_PRE_MEASUREMENT_TIME_MS
  *       by at least one single shot duration (~5 s) and leave ~5 s before the uplink
  *       for the second shot to settle during low-power sleep.
  */
#define SCD41_RESTART_TIME_MS                       6000

/**
  * @brief SPS30 pre-measurement time in milliseconds.
  * @note This is the time the node wakes up before the uplink to start the particulate matter measurement.
  */
#define SPS30_PRE_MEASUREMENT_TIME_MS               16500

/**
  * @brief SPS30 fan cleaning duration in milliseconds.
  */
#define SPS30_CLEANING_DURATION_MS                  10500

/* USER CODE END EC_CAYENNE_LPP */
/*!
 * LoRaWAN User application port
 * @note do not use 224. It is reserved for certification
 */
#define LORAWAN_USER_APP_PORT                       2

/**
 * Crystal error of the MCU to fine adjust the rx window for lorawan
 * ( ex: set 30 for a crystal error = 0.3%).
 * Default value is 10
 */
#define BSP_CRYSTAL_ERROR                           10

/*!
 * LoRaWAN Certification Mode
 * @note It is disabled by default
 */
#define LORAWAN_CERTIFICATION_MODE                  false

/*!
 * User application data buffer size
 */
#define LORAWAN_APP_DATA_BUFFER_MAX_SIZE            242

/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macros -----------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Init Lora Application
  */
void LoRaWAN_Init(void);

/**
  * @brief  Lora Application Process
  */
void LoRaWAN_Process(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*__LORA_APP_H__*/
