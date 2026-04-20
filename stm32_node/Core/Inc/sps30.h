/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sps30.h
  * @author  Gemini CLI
  * @brief   Header for SPS30 particulate matter sensor driver
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPS30_H__
#define __SPS30_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
#define SPS30_I2C_ADDR                  0x69U
#define SPS30_I2C_ADDR_8BIT             (SPS30_I2C_ADDR << 1)

/* SPS30 Status codes */
#define SPS30_STATUS_OK                 0
#define SPS30_STATUS_ERROR             -1
#define SPS30_STATUS_CRC_ERROR         -2

/* SPS30 Timing */
#define SPS30_WAKEUP_DELAY_MS           5U
#define SPS30_START_MEASUREMENT_DELAY_MS 20U
#define SPS30_I2C_TIMEOUT_MS            100U

/* Exported types ------------------------------------------------------------*/
typedef struct {
  uint16_t mc_1_0;   /*!< Mass Concentration PM1.0  [0.1 ug/m3] (ug/m3 * 10) */
  uint16_t mc_2_5;   /*!< Mass Concentration PM2.5  [0.1 ug/m3] (ug/m3 * 10) */
  uint16_t mc_4_0;   /*!< Mass Concentration PM4.0  [0.1 ug/m3] (ug/m3 * 10) */
  uint16_t mc_10_0;  /*!< Mass Concentration PM10.0 [0.1 ug/m3] (ug/m3 * 10) */
} SPS30_Data_t;

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Initializes the SPS30 sensor (checks connectivity, enters sleep)
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_Init(void);

/**
  * @brief  Wakes up the SPS30 sensor from sleep mode
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_WakeUp(void);

/**
  * @brief  Starts the measurement mode
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_StartMeasurement(void);

/**
  * @brief  Reads the measurement results from the sensor
  * @param  data Pointer to SPS30_Data_t structure
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_ReadMeasurement(SPS30_Data_t *data);

/**
  * @brief  Stops the measurement mode
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_StopMeasurement(void);

/**
  * @brief  Sets the fan auto cleaning interval
  * @param  interval_s Interval in seconds (0 to disable)
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_SetFanAutoCleaningInterval(uint32_t interval_s);

/**
  * @brief  Starts the fan cleaning manually
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_StartFanCleaning(void);

/**
  * @brief  Enters sleep mode (low power)
  * @retval SPS30_STATUS_OK if successful
  */
int32_t SPS30_Sleep(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPS30_H__ */
