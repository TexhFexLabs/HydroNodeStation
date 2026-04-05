/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lc709203f.h
  * @author  MCD Application Team
  * @brief   LC709203F battery monitor low-level driver
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LC709203F_H__
#define __LC709203F_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
#define LC709203F_STATUS_OK            0
#define LC709203F_STATUS_ERROR        -1
#define LC709203F_STATUS_CRC_ERROR    -2

/* Exported functions prototypes ---------------------------------------------*/
int32_t LC709203F_Init(void);
int32_t LC709203F_ReadVoltageMv(uint16_t *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* __LC709203F_H__ */
