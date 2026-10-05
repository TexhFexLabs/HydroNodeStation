/**
  ******************************************************************************
  * @file    stm32wlxx_nucleo_radio.c
  * @author  MCD Application Team
  * @brief   This file provides set of firmware functions to manage:
  *          - RF circuitry available on STM32WLXX-LoRa_E5_mini
  *            Kit from STMicroelectronics
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020(-2021) STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32wlxx_LoRa_E5_mini_radio.h"
#include <stdbool.h>

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM32WLXX_LoRa_E5_mini
  * @{
  */

/** @addtogroup STM32WLXX_LoRa_E5_mini_RADIO_LOW_LEVEL
  * @brief This file provides set of firmware functions to Radio switch 
  *        available on STM32WLXX-Nucleo Kit from STMicroelectronics.
  * @{
  */

/** @addtogroup STM32WLXX_LoRa_E5_mini_RADIO_LOW_LEVEL_Exported_Functions
  * @{
  */
  
/**
  * @brief  Init Radio Switch 
  * @retval BSP status
  */
#if !defined(USE_RF_SW_SINGLE_CTRL_PC13) || (USE_RF_SW_SINGLE_CTRL_PC13 != 1U)
#error "PCB 1.1 drives the BGS12SN6 with a single CTRL line on PC13"
#endif

/* PCB 1.1: BGS12SN6 with VDD through R14 = 470 Ohm and CTRL through
 * R15 = 470 Ohm, each with 101 nF (C13+C14, C15+C16); tau = 47.5 us.
 * RF1 (CTRL LOW) = RX, RF2 (CTRL HIGH) = TX.
 * The switch is powered only while the radio works: LBM calls
 * smtc_modem_hal_start_radio_tcxo() before every TX/RX launch and
 * smtc_modem_hal_stop_radio_tcxo() after putting the radio to sleep; both
 * land here. CTRL must never be HIGH while VDD is off, or current flows
 * through CTRL into the unpowered switch. */
#define RF_SW_VDD_SETTLE_US   1000U  /* 5 tau = 240 us + tPUP 15 us, with margin */
#define RF_SW_CTRL_SETTLE_US  250U   /* 99 % after about 220 us */

static bool rf_sw_powered;
static bool rf_sw_ctrl_high;
/* Set by RADIO_SWITCH_OFF (radio sleep), cleared by the next power-on. The
 * radio planner selects RX right after every sleep; that must not power the
 * switch up again. */
static bool radio_asleep = true;

/* Busy wait of at least us microseconds: one loop takes 4 or more cycles. */
static void rf_sw_wait_us(uint32_t us)
{
  for (volatile uint32_t n = us * (SystemCoreClock / 4000000U); n != 0U; --n) { }
}

static void rf_sw_set_ctrl(bool high)
{
  if (high == rf_sw_ctrl_high) return;
  HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
  rf_sw_ctrl_high = high;
  rf_sw_wait_us(RF_SW_CTRL_SETTLE_US);
}

int32_t BSP_RADIO_Init(void)
{
  GPIO_InitTypeDef  gpio_init_structure = {0};

  /* Both lines driven LOW: switch unpowered until the first radio task.
   * PB0 is VDD_TCXO and unused on PCB 1.1. */
  RF_SW_CTRL_GPIO_CLK_ENABLE();
  RF_SW_VDD_CLK_ENABLE();
  HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RF_SW_VDD_GPIO_PORT, RF_SW_VDD_PIN, GPIO_PIN_RESET);
  gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_init_structure.Pull  = GPIO_NOPULL;
  gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init_structure.Pin   = RF_SW_CTRL_PIN;
  HAL_GPIO_Init(RF_SW_CTRL_GPIO_PORT, &gpio_init_structure);
  gpio_init_structure.Pin   = RF_SW_VDD_PIN;
  HAL_GPIO_Init(RF_SW_VDD_GPIO_PORT, &gpio_init_structure);
  rf_sw_powered = false;
  rf_sw_ctrl_high = false;
  radio_asleep = true;

  return BSP_ERROR_NONE;
}

int32_t BSP_RADIO_DeInit(void)
{
  BSP_RADIO_SwitchPowerOff();
  /* CTRL may float; VDD stays actively LOW so C13/C14 discharge through R14. */
  HAL_GPIO_DeInit(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN);

  return BSP_ERROR_NONE;
}

void BSP_RADIO_SwitchPowerOn(void)
{
  radio_asleep = false;
  if (rf_sw_powered) return;
  /* CTRL LOW -> VDD HIGH -> wait until VDD has settled. */
  HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_RESET);
  rf_sw_ctrl_high = false;
  HAL_GPIO_WritePin(RF_SW_VDD_GPIO_PORT, RF_SW_VDD_PIN, GPIO_PIN_SET);
  rf_sw_powered = true;
  rf_sw_wait_us(RF_SW_VDD_SETTLE_US);
}

void BSP_RADIO_SwitchPowerOff(void)
{
  radio_asleep = true;
  /* CTRL LOW (and settled) first, then VDD actively LOW, not floating. */
  rf_sw_set_ctrl(false);
  if (!rf_sw_powered) return;
  HAL_GPIO_WritePin(RF_SW_VDD_GPIO_PORT, RF_SW_VDD_PIN, GPIO_PIN_RESET);
  rf_sw_powered = false;
}

uint32_t BSP_RADIO_SwitchStartupDelayMs(void)
{
  /* VDD settle plus one CTRL settle, rounded up with margin. */
  return 2U;
}

int32_t BSP_RADIO_ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config)
{
  switch (Config)
  {
    case RADIO_SWITCH_OFF:
    {
      BSP_RADIO_SwitchPowerOff();
      break;
    }
    case RADIO_SWITCH_RX:
    {
      /* RX = CTRL LOW, which is also the safe state while unpowered. */
      if (!rf_sw_powered && !radio_asleep)
      {
        BSP_RADIO_SwitchPowerOn();
      }
      rf_sw_set_ctrl(false);
      break;
    }
    case RADIO_SWITCH_RFO_LP:
    case RADIO_SWITCH_RFO_HP:
    {
      /* TX = CTRL HIGH, only with VDD up. */
      if (!rf_sw_powered)
      {
        BSP_RADIO_SwitchPowerOn();
      }
      rf_sw_set_ctrl(true);
      break;
    }
    default:
      break;
  }

  return BSP_ERROR_NONE;
}

/**
  * @brief  Return Board Configuration
  * @retval 
  *  RADIO_CONF_RFO_LP_HP
  *  RADIO_CONF_RFO_LP
  *  RADIO_CONF_RFO_HP
  */
int32_t BSP_RADIO_GetTxConfig(void)
{
  return RADIO_CONF_RFO_HP;
}

/**
  * @brief  Get If TCXO is to be present on board
  * @note   never remove called by MW,
  * @retval
  *  RADIO_CONF_TCXO_NOT_SUPPORTED
  *  RADIO_CONF_TCXO_SUPPORTED
  */
int32_t BSP_RADIO_IsTCXO(void)
{
  return RADIO_CONF_TCXO_NOT_SUPPORTED;
}

/**
  * @brief  Get If DCDC is to be present on board
  * @note   never remove called by MW,
  * @retval
  *  RADIO_CONF_DCDC_NOT_SUPPORTED
  *  RADIO_CONF_DCDC_SUPPORTED  
  */
int32_t BSP_RADIO_IsDCDC(void)
{
  return RADIO_CONF_DCDC_SUPPORTED;
}

/**
  * @brief  Return RF Output Max Power Configuration
  * @retval
  *    RADIO_CONF_RFO_LP_MAX_15_dBm for LP mode
  *    RADIO_CONF_RFO_HP_MAX_22_dBm for HP mode
  */
int32_t BSP_RADIO_GetRFOMaxPowerConfig(BSP_RADIO_RFOMaxPowerConfig_TypeDef Config)
{
  int32_t ret;

  if(Config == RADIO_RFO_LP_MAXPOWER)
  {
    ret = RADIO_CONF_RFO_LP_MAX_15_dBm;
  }
  else
  {
    ret = RADIO_CONF_RFO_HP_MAX_22_dBm;
  }

  return ret;
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */    

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
