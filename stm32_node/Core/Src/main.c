/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "i2c.h"
#include "app_lorawan.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "debug_profile.h"
#include "runtime_health.h"
#include "power_rail.h"
#include "pulse_counter.h"
#include "standby.h"
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Per attempt. The LSE crystal U14 sits 12 mm from PC14/PC15 with vias on
 * Inner2 (PCB 1.1 finding P2), so its start is given a second chance. */
#define LSE_START_TIMEOUT_MS 2000U
/* Without the LSE the RTC cannot time the RX windows: show the code, then
 * retry from a reset; a later cold start may succeed. */
#define LSE_FAULT_HOLD_S     60U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t lse_attempts;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void LSE_Start(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  Runtime_EarlyInit();
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* After a Standby wake-up: back to Standby while the battery is low,
   * before any rail or sensor draws current. */
  Standby_CheckWake();
  /* +5 V for the SPS30 before any sensor init, also when booting into
   * RECOVERY or the debug profile; 3V3SWITCHABLE stays off. */
  PowerRail_Init();

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_LoRaWAN_Init();
  /* USART1 (H1) is started by the trace driver or the DIP 3 profile only. */
  /* USER CODE BEGIN 2 */
  PulseCounter_Init();
  /* Debug profile: switch on PB4 → read all sensors forever, skip LoRaWAN.
   * MX_LoRaWAN_Init() above already called SystemApp_Init() (trace up) and
   * LoRaWAN_Init() (sensors init'd), so APP_LOG and EnvSensors_Read are ready. */
  if (HAL_GPIO_ReadPin(DIP3_DEBUG_GPIO_Port, DIP3_DEBUG_Pin) == GPIO_PIN_SET)
  {
    DebugProfile_Run(); /* never returns */
  }
  {
    uint32_t startTick = HAL_GetTick();
    while ((HAL_GetTick() - startTick) < 10000U)
    {
      HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
      HAL_Delay(1000U);
    }
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_LoRaWAN_Process();
    Runtime_Process();

    /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Backup domain access for the LSE, started below in LSE_Start()
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SystemClock_Config_LSE */
  LSE_Start();
  /* USER CODE END SystemClock_Config_LSE */
}

/* USER CODE BEGIN 4 */
static bool LSE_Try(uint32_t drive)
{
  RCC_OscInitTypeDef osc = {0};

  /* The LSE lives in the backup domain and survives a system reset; keep it
   * running then (its drive cannot be raised while it is on anyway). */
  if (!LL_RCC_LSE_IsReady())
  {
    LL_RCC_LSE_Disable();
    LL_RCC_LSE_SetDriveCapability(drive);
    LL_RCC_LSE_Enable();
    uint32_t start = HAL_GetTick();
    while (!LL_RCC_LSE_IsReady())
    {
      if ((HAL_GetTick() - start) > LSE_START_TIMEOUT_MS)
      {
        LL_RCC_LSE_Disable();
        return false;
      }
    }
  }
  /* Ready now: the HAL only adds LSESYSEN and returns at once. */
  osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  osc.LSEState = RCC_LSE_ON;
  return HAL_RCC_OscConfig(&osc) == HAL_OK;
}

/* MEDIUMHIGH drive is sufficient by calculation (F18); HIGH is the fallback.
 * No LSI fallback: the LSI is too inaccurate for the RX windows. */
static void LSE_Start(void)
{
  lse_attempts = 1U;
  if (LSE_Try(RCC_LSEDRIVE_MEDIUMHIGH)) return;
  lse_attempts = 2U;
  if (LSE_Try(RCC_LSEDRIVE_HIGH)) return;
  Runtime_EarlyFault(RUNTIME_FAULT_LSE, LSE_FAULT_HOLD_S);
}

uint8_t SystemClock_LseAttempts(void)
{
  return lse_attempts;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  Runtime_Fault(RUNTIME_FAULT_HAL);
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
