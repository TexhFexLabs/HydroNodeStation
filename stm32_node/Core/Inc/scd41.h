/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scd41.h
  * @author  MCD Application Team
  * @brief   SCD41 CO2 sensor low-level driver
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SCD41_H__
#define __SCD41_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
#define SCD41_STATUS_OK            0
#define SCD41_STATUS_ERROR        -1
#define SCD41_STATUS_CRC_ERROR    -2

/* Exported functions prototypes ---------------------------------------------*/
int32_t SCD41_Init(void);
/* temperature: 0.01 degC (degC*100), humidity: 0.01 % (%*100) */
int32_t SCD41_ReadRhtSingleShot(int16_t *temperature, uint16_t *humidity);
int32_t SCD41_StartCo2SingleShot(void);
int32_t SCD41_DiscardAndRestartCo2SingleShot(void);
int32_t SCD41_ReadCo2SingleShot(uint16_t *co2_ppm, int16_t *temperature, uint16_t *humidity);

#ifdef __cplusplus
}
#endif

#endif /* __SCD41_H__ */
