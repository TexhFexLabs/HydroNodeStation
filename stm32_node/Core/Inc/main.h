/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wlxx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* 1 when the LSE started at MEDIUMHIGH drive (or was already running),
 * 2 when it needed the HIGH-drive retry. */
uint8_t SystemClock_LseAttempts(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RTC_N_PREDIV_S 10
#define RTC_PREDIV_S ((1<<RTC_N_PREDIV_S)-1)
#define RTC_PREDIV_A ((1<<(15-RTC_N_PREDIV_S))-1)
#define RTC_PREDIV_A ((1<<(15-RTC_N_PREDIV_S))-1)
#define RTC_N_PREDIV_S 10
#define RTC_PREDIV_S ((1<<RTC_N_PREDIV_S)-1)

/* USER CODE BEGIN Private defines */
/* PCB 1.1 (hardware revision 2) pin map, named after the schematic nets.
 * Unused pins (PA1, PA3 1-Wire, PA6, PA7, PA10, PA15, PB0 VDD_TCXO) stay
 * analog; see MX_GPIO_Init(). PA13/PA14 remain SWD. */

/* LEDs: VCC -> 1 kOhm -> LED -> DIP -> pin, active low and only lit while
 * the matching DIP switch is closed. */
#define LED1_Pin            GPIO_PIN_5      /* PB5, via DIP 1 */
#define LED1_GPIO_Port      GPIOB
/* Second LED, used only for fault diagnostics so the startup blink on LED1
 * stays readable. PB3 is JTDO-TRACESWO by default; SWO is unused here. */
#define LED_DIAG_Pin        GPIO_PIN_3      /* PB3, via DIP 2 */
#define LED_DIAG_GPIO_Port  GPIOB

/* DIP 3 (debug profile) and DIP 4 (installation mode) switch to GND. */
#define DIP3_DEBUG_Pin          GPIO_PIN_4  /* PB4 */
#define DIP3_DEBUG_GPIO_Port    GPIOB
#define DIP4_INSTALL_Pin        GPIO_PIN_8  /* PB8 */
#define DIP4_INSTALL_GPIO_Port  GPIOB

/* MAX17048 ALRT, open drain with an external pull-up. Not used. */
#define MAX17048_ALRT_Pin       GPIO_PIN_0  /* PA0 */
#define MAX17048_ALRT_GPIO_Port GPIOA

/* BQ25185 CE, 10 kOhm pull-down: charging is enabled while the pin floats. */
#define CHG_CE_Pin          GPIO_PIN_2      /* PA2 */
#define CHG_CE_GPIO_Port    GPIOA

/* Contact inputs on P4: 1 kOhm series, 1 MOhm pull-up to VCC, 1 nF to GND. */
#define CNT1_Pin            GPIO_PIN_4      /* PA4, EXTI4 */
#define CNT1_GPIO_Port      GPIOA
#define CNT2_Pin            GPIO_PIN_5      /* PA5, LPTIM2_ETR / EXTI5 */
#define CNT2_GPIO_Port      GPIOA

/* Rail enables, 1 MOhm pull-down each: both rails are off without firmware. */
#define EN3V3SW_Pin         GPIO_PIN_8      /* PA8, U1 3V3SWITCHABLE */
#define EN3V3SW_GPIO_Port   GPIOA
#define EN5V_Pin            GPIO_PIN_9      /* PA9, U4 +5 V */
#define EN5V_GPIO_Port      GPIOA

/* Solar divider 150 kOhm / 100 kOhm (factor 2.5), ADC_IN4. */
#define SOLAR_ADC_Pin       GPIO_PIN_2      /* PB2 */
#define SOLAR_ADC_GPIO_Port GPIOB

/* USART1 on H1, also the system bootloader port. */
#define DBG_UART_TX_Pin     GPIO_PIN_6      /* PB6 */
#define DBG_UART_RX_Pin     GPIO_PIN_7      /* PB7 */
#define DBG_UART_GPIO_Port  GPIOB

/* BGS12SN6 RF switch: VDD through 470 Ohm, CTRL through 470 Ohm. */
#define RF_SW_VDD_Pin       GPIO_PIN_12     /* PB12 */
#define RF_SW_VDD_GPIO_Port GPIOB
#define RF_SW_CTRL_Pin      GPIO_PIN_13     /* PC13, driven by the radio BSP */
#define RF_SW_CTRL_GPIO_Port GPIOC

/* I2C2: PA11 SDA, PA12 SCL (see i2c.c). */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
