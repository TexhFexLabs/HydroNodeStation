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

  /* LED1 (PB5) and LED_DIAG (PB3) stay analog: active low through a DIP, an
   * output HIGH would leak nothing useful and a pattern drives them only
   * while it runs (led.c, Runtime_Fault). */
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin = LED1_Pin | LED_DIAG_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
  GPIO_InitStruct.Pin = GPIO_PIN_0 | SOLAR_ADC_Pin | DBG_UART_TX_Pin | DBG_UART_RX_Pin
                      | DIP3_DEBUG_Pin | DIP4_INSTALL_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI4 / EXTI9_5 for the contact counters: PulseCounter_Init(). */

}

/* USER CODE BEGIN 2 */
/* DIP 3 (PB4) and DIP 4 (PB8) switch to GND. Read once at boot with the
 * internal pull-up, then back to analog: a pull-up left on would draw up to
 * 132 uA through a closed switch for as long as the station runs. */
uint8_t DIP_Read(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint8_t dip = 0U;

  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitStruct.Pin = DIP3_DEBUG_Pin | DIP4_INSTALL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_Delay(2U);
  if (HAL_GPIO_ReadPin(DIP3_DEBUG_GPIO_Port, DIP3_DEBUG_Pin) == GPIO_PIN_RESET) dip |= DIP_DEBUG;
  if (HAL_GPIO_ReadPin(DIP4_INSTALL_GPIO_Port, DIP4_INSTALL_Pin) == GPIO_PIN_RESET) dip |= DIP_INSTALL;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  return dip;
}
/* USER CODE END 2 */
