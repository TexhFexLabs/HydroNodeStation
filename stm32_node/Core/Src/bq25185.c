#include "bq25185.h"

void BQ25185_ChargeDisable(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Set output level before switching the pin to output to avoid a low glitch */
  BQ25185_CE_PORT->BSRR = BQ25185_CE_PIN;

  GPIO_InitStruct.Pin = BQ25185_CE_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BQ25185_CE_PORT, &GPIO_InitStruct);
}

void BQ25185_ChargeEnable(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Release CE: the charger's internal pull-down takes it low (charging on) */
  GPIO_InitStruct.Pin = BQ25185_CE_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BQ25185_CE_PORT, &GPIO_InitStruct);
}
