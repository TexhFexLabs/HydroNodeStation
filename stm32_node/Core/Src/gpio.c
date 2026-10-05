/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_DIAG_Pin (PB3)
   * Diagnostic LED, driven only by Runtime_Fault. Same active-low wiring as
   * LED1, so it is parked HIGH (off) here. */
  HAL_GPIO_WritePin(LED_DIAG_GPIO_Port, LED_DIAG_Pin, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = LED_DIAG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_DIAG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DIP3_DEBUG_Pin (PB4)
   * Switch ON ties PB4 to VCC → reads HIGH → debug profile active.
   * Pull-down ensures LOW when switch is open. */
  GPIO_InitStruct.Pin  = DIP3_DEBUG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(DIP3_DEBUG_GPIO_Port, &GPIO_InitStruct);

  /* Unused and not yet driven pins are analog: no input buffer, no leakage.
   * PA0 (MAX17048 ALRT) has an external pull-up and is not used, so no EXTI
   * and no internal pull-up. PA4/PA5 (contact inputs) stay analog unless the
   * pulse counters are built in. PA15/PB4 leave their JTAG pull-ups behind.
   * PB0 is VDD_TCXO on the module footprint and must never be driven. */
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin = MAX17048_ALRT_Pin | GPIO_PIN_1 | GPIO_PIN_3 | CNT1_Pin | CNT2_Pin
                      | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_10 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = GPIO_PIN_0 | SOLAR_ADC_Pin | DBG_UART_TX_Pin | DBG_UART_RX_Pin | DIP4_INSTALL_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI4 / EXTI9_5 for the contact counters: PulseCounter_Init(). */

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
