#include "power_rail.h"

static bool rail_on[2];

/* Push-pull with the level written before the mode switch, so the enable
 * never glitches when the pin leaves analog/reset state. */
static void rail_drive(GPIO_TypeDef *port, uint16_t pin, bool on)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(port, &GPIO_InitStruct);
}

void PowerRail_Init(void)
{
  PowerRail_3V3Sw_Deactivate();
  PowerRail_5V_Activate();
  HAL_Delay(POWER_RAIL_SPS30_BOOT_MS);
}

void PowerRail_3V3Sw_Activate(void)
{
  rail_drive(EN3V3SW_GPIO_Port, EN3V3SW_Pin, true);
  if (!rail_on[POWER_RAIL_3V3SW]) HAL_Delay(POWER_RAIL_SETTLE_MS);
  rail_on[POWER_RAIL_3V3SW] = true;
}

void PowerRail_3V3Sw_Deactivate(void)
{
  rail_drive(EN3V3SW_GPIO_Port, EN3V3SW_Pin, false);
  rail_on[POWER_RAIL_3V3SW] = false;
}

void PowerRail_5V_Activate(void)
{
  rail_drive(EN5V_GPIO_Port, EN5V_Pin, true);
  if (!rail_on[POWER_RAIL_5V]) HAL_Delay(POWER_RAIL_SETTLE_MS);
  rail_on[POWER_RAIL_5V] = true;
}

void PowerRail_5V_Deactivate(void)
{
  rail_drive(EN5V_GPIO_Port, EN5V_Pin, false);
  rail_on[POWER_RAIL_5V] = false;
}

bool PowerRail_IsOn(power_rail_t rail)
{
  return rail_on[rail];
}
