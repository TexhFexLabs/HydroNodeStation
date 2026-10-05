/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc_if.c
  * @author  MCD Application Team
  * @brief   Read status related to the chip (battery level, VREF, chip temperature)
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
#include "adc_if.h"
#include "sys_app.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/**
  * @brief ADC handle
  */
extern ADC_HandleTypeDef hadc;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
#define TEMPSENSOR_TYP_CAL1_V          (( int32_t)  760)        /*!< Internal temperature sensor, parameter V30 (unit: mV). Refer to device datasheet for min/typ/max values. */
#define TEMPSENSOR_TYP_AVGSLOPE        (( int32_t) 2500)        /*!< Internal temperature sensor, parameter Avg_Slope (unit: uV/DegCelsius). Refer to device datasheet for min/typ/max values. */

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief This function reads the ADC channel
  * @param channel channel number to read
  * @return adc measured level value
  */
static uint32_t ADC_ReadChannels(uint32_t channel);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
/* USER CODE BEGIN EF */
/* Solar divider R32 = 150 kOhm / R33 = 100 kOhm on PB2 (ADC_IN4). */
#define SOLAR_DIVIDER_NUM      5U
#define SOLAR_DIVIDER_DEN      2U
#define ADC_SAMPLES            4U
#define ADC_FULL_SCALE         4095U

/* VDDA from the factory VREFINT calibration (taken at VDDA = 3.3 V). */
static uint32_t adc_vdda_mv(uint32_t vref_raw, uint32_t vref_cal)
{
  return (vref_raw == 0U) ? 0U : (VREFINT_CAL_VREF * vref_cal + vref_raw / 2U) / vref_raw;
}

/* Panel voltage behind the 2.5:1 divider, in mV. */
static uint16_t adc_panel_mv(uint32_t pin_raw, uint32_t vdda_mv)
{
  uint32_t mv = (pin_raw * vdda_mv * SOLAR_DIVIDER_NUM + ADC_FULL_SCALE) / (ADC_FULL_SCALE * SOLAR_DIVIDER_DEN);
  return (mv > 0xFFFEU) ? 0xFFFEU : (uint16_t)mv;
}

/* One ADC session: calibrate, convert VREFINT and one channel ADC_SAMPLES
 * times each with the longest sampling time (160.5 cycles at 12 MHz, the
 * divider has about 60 kOhm source impedance plus C28), then switch the ADC
 * off again. Unlike ADC_ReadChannels() a HAL error is not fatal. */
static bool adc_session(uint32_t channel, uint32_t *vref_avg, uint32_t *ch_avg)
{
  const uint32_t channels[2] = { ADC_CHANNEL_VREFINT, channel };
  uint32_t sum[2] = { 0U, 0U };
  ADC_ChannelConfTypeDef sConfig = {0};
  bool ok = true;

  hadc.Instance = ADC;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.NbrOfConversion = 1;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5;
  hadc.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
  hadc.Init.OversamplingMode = DISABLE;
  hadc.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc) != HAL_OK || HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK)
  {
    ok = false;
  }
  for (uint8_t c = 0U; ok && c < 2U; ++c)
  {
    sConfig.Channel = channels[c];
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
    {
      ok = false;
      break;
    }
    for (uint8_t i = 0U; i < ADC_SAMPLES; ++i)
    {
      if (HAL_ADC_Start(&hadc) != HAL_OK || HAL_ADC_PollForConversion(&hadc, 10U) != HAL_OK)
      {
        ok = false;
        break;
      }
      sum[c] += HAL_ADC_GetValue(&hadc);
      (void)HAL_ADC_Stop(&hadc);
    }
  }
  (void)HAL_ADC_Stop(&hadc);
  (void)HAL_ADC_DeInit(&hadc);
  if (!ok) return false;
  *vref_avg = (sum[0] + ADC_SAMPLES / 2U) / ADC_SAMPLES;
  *ch_avg = (sum[1] + ADC_SAMPLES / 2U) / ADC_SAMPLES;
  return *vref_avg != 0U;
}

bool SYS_ReadSolarMv(uint16_t *panel_mv)
{
  uint32_t vref, pin;
  if (panel_mv == NULL || !adc_session(ADC_CHANNEL_4, &vref, &pin)) return false;
  *panel_mv = adc_panel_mv(pin, adc_vdda_mv(vref, *VREFINT_CAL_ADDR));
  return true;
}

/* USER CODE END EF */

void SYS_InitMeasurement(void)
{
  /* USER CODE BEGIN SYS_InitMeasurement_1 */

  /* USER CODE END SYS_InitMeasurement_1 */
  hadc.Instance = ADC;
  /* USER CODE BEGIN SYS_InitMeasurement_2 */

  /* USER CODE END SYS_InitMeasurement_2 */
}

void SYS_DeInitMeasurement(void)
{
  /* USER CODE BEGIN SYS_DeInitMeasurement_1 */

  /* USER CODE END SYS_DeInitMeasurement_1 */
}

int16_t SYS_GetTemperatureLevel(void)
{
  /* USER CODE BEGIN SYS_GetTemperatureLevel_1 */

  /* USER CODE END SYS_GetTemperatureLevel_1 */
  __IO int16_t temperatureDegreeC = 0;
  uint32_t measuredLevel = 0;
  uint16_t batteryLevelmV = SYS_GetBatteryLevel();

  measuredLevel = ADC_ReadChannels(ADC_CHANNEL_TEMPSENSOR);

  /* convert ADC level to temperature */
  /* check whether device has temperature sensor calibrated in production */
  if (((int32_t)*TEMPSENSOR_CAL2_ADDR - (int32_t)*TEMPSENSOR_CAL1_ADDR) != 0)
  {
    /* Device with temperature sensor calibrated in production:
       use device optimized parameters */
    temperatureDegreeC = __LL_ADC_CALC_TEMPERATURE(batteryLevelmV,
                                                   measuredLevel,
                                                   LL_ADC_RESOLUTION_12B);
  }
  else
  {
    /* Device with temperature sensor not calibrated in production:
       use generic parameters */
    temperatureDegreeC = __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(TEMPSENSOR_TYP_AVGSLOPE,
                                                              TEMPSENSOR_TYP_CAL1_V,
                                                              TEMPSENSOR_CAL1_TEMP,
                                                              batteryLevelmV,
                                                              measuredLevel,
                                                              LL_ADC_RESOLUTION_12B);
  }

  /* from int16 to q8.7*/
  temperatureDegreeC <<= 8;

  return (int16_t) temperatureDegreeC;
  /* USER CODE BEGIN SYS_GetTemperatureLevel_2 */

  /* USER CODE END SYS_GetTemperatureLevel_2 */
}

uint16_t SYS_GetBatteryLevel(void)
{
  /* USER CODE BEGIN SYS_GetBatteryLevel_1 */

  /* USER CODE END SYS_GetBatteryLevel_1 */
  uint16_t batteryLevelmV = 0;
  uint32_t measuredLevel = 0;

  measuredLevel = ADC_ReadChannels(ADC_CHANNEL_VREFINT);

  if (measuredLevel == 0)
  {
    batteryLevelmV = 0;
  }
  else
  {
    if ((uint32_t)*VREFINT_CAL_ADDR != (uint32_t)0xFFFFU)
    {
      /* Device with Reference voltage calibrated in production:
         use device optimized parameters */
      batteryLevelmV = __LL_ADC_CALC_VREFANALOG_VOLTAGE(measuredLevel,
                                                        ADC_RESOLUTION_12B);
    }
    else
    {
      /* Device with Reference voltage not calibrated in production:
         use generic parameters */
      batteryLevelmV = (VREFINT_CAL_VREF * 1510) / measuredLevel;
    }
  }

  return batteryLevelmV;
  /* USER CODE BEGIN SYS_GetBatteryLevel_2 */

  /* USER CODE END SYS_GetBatteryLevel_2 */
}

/* Private Functions Definition -----------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

static uint32_t ADC_ReadChannels(uint32_t channel)
{
  /* USER CODE BEGIN ADC_ReadChannels_1 */

  /* USER CODE END ADC_ReadChannels_1 */
  uint32_t ADCxConvertedValues = 0;
  ADC_ChannelConfTypeDef sConfig = {0};

  MX_ADC_Init();

  /* Start Calibration */
  if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure Regular Channel */
  sConfig.Channel = channel;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_Start(&hadc) != HAL_OK)
  {
    /* Start Error */
    Error_Handler();
  }
  /** Wait for end of conversion */
  HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);

  /** Wait for end of conversion */
  HAL_ADC_Stop(&hadc);   /* it calls also ADC_Disable() */

  ADCxConvertedValues = HAL_ADC_GetValue(&hadc);

  HAL_ADC_DeInit(&hadc);

  return ADCxConvertedValues;
  /* USER CODE BEGIN ADC_ReadChannels_2 */

  /* USER CODE END ADC_ReadChannels_2 */
}
