/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.c
  * @author  MCD Application Team
  * @brief   Manages the sensors on the application
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
#include "stdint.h"
#include "platform.h"
#include "sys_conf.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */
#include "sht45.h"
#include "bmp390.h"
#include "ltr390.h"
#include "max17048.h"
#include "scd41.h"
#include "sps30.h"
#include "i2c.h"
#include "sys_app.h"
#include "adc_if.h"
#include "ina226.h"
#include "fault_report.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/

/* USER CODE BEGIN PD */
/* 7-bit addresses for the "not found" detail on fPort 99. */
#define ADDR_SHT45     0x44U
#define ADDR_BMP390    0x77U
#define ADDR_LTR390    0x53U
#define ADDR_SCD41     0x62U
#define ADDR_MAX17048  0x36U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void note_result(uint8_t comp, bool ok);
/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
int32_t EnvSensors_Read(sensor_t *sensor_data, uint8_t sensor_flags)
{
  /* USER CODE BEGIN EnvSensors_Read */
  if (sensor_data == NULL)
  {
    return -1;
  }

  sensor_data->valid = 0U;
  /* Invalid sentinels are distinct from legitimate zero readings. */
  sensor_data->humidity         = SENSOR_INVALID_U16;
  sensor_data->temperature      = SENSOR_INVALID_T;
  sensor_data->pressure         = SENSOR_INVALID_U16;
  sensor_data->battery_voltage  = SENSOR_INVALID_U16;
  sensor_data->uvi_x100         = SENSOR_INVALID_U16;
  sensor_data->co2_ppm          = SENSOR_INVALID_U16;
  sensor_data->pm1_0            = SENSOR_INVALID_U16;
  sensor_data->pm2_5            = SENSOR_INVALID_U16;
  sensor_data->pm4_0            = SENSOR_INVALID_U16;
  sensor_data->pm10_0           = SENSOR_INVALID_U16;
  sensor_data->nc_0_5           = SENSOR_INVALID_U16;
  sensor_data->nc_1_0           = SENSOR_INVALID_U16;
  sensor_data->nc_2_5           = SENSOR_INVALID_U16;
  sensor_data->nc_4_0           = SENSOR_INVALID_U16;
  sensor_data->nc_10_0          = SENSOR_INVALID_U16;
  sensor_data->typ_size         = SENSOR_INVALID_U16;

  /* 1. Read MAX17048 */
  MAX17048_Data_t max17048;
  uint32_t gauge_error = 0U;
  if (MAX17048_Read(&max17048) == MAX17048_OK)
  {
    sensor_data->battery_voltage = max17048.voltage_mv;
    sensor_data->valid |= SENSOR_VALID_BATTERY;
  }
  else
  {
    /* Try to recover I2C and re-init fuel gauge when first read fails at boot. */
    gauge_error = HAL_I2C_GetError(&hi2c2);
    I2C2_RecoverBus();
    if ((MAX17048_Init() == MAX17048_OK) && (MAX17048_Read(&max17048) == MAX17048_OK))
    {
      sensor_data->battery_voltage = max17048.voltage_mv;
      sensor_data->valid |= SENSOR_VALID_BATTERY;
    }
  }
  Fault_ComponentResult(FAULT_COMP_MAX17048, (sensor_data->valid & SENSOR_VALID_BATTERY) != 0U,
                        (sensor_data->valid & SENSOR_VALID_BATTERY) != 0U ? 0U : gauge_error);

  if (sensor_flags & SENSOR_FLAG_ONLY_BATTERY)
  {
    return 0;
  }

  /* 2. Read SHT45 */
  SHT45_Data_t sht45;
  if (SHT45_Read(&sht45) == SHT45_OK)
  {
    sensor_data->valid |= SENSOR_VALID_RHT;
    sensor_data->temperature = sht45.temperature;   /* 0.01 degC */
    sensor_data->humidity    = sht45.humidity;      /* 0.01 %RH  */
    note_result(FAULT_COMP_SHT45, true);
  }
  else
  {
    note_result(FAULT_COMP_SHT45, false);
    I2C2_RecoverBus();
  }

  /* 3. Read BMP390 */
  BMP390_Data_t bmp390;
  if (BMP390_Read(&bmp390) == BMP390_OK)
  {
    sensor_data->valid |= SENSOR_VALID_PRESSURE;
    sensor_data->pressure = bmp390.pressure_hPa;  /* 0.1 hPa */
    note_result(FAULT_COMP_BMP390, true);
  }
  else
  {
    note_result(FAULT_COMP_BMP390, false);
    I2C2_RecoverBus();
  }

  /* 4. Read LTR390 */
  LTR390_Data_t ltr390;
  if (LTR390_ReadUV(&ltr390) == LTR390_OK)
  {
    sensor_data->valid |= SENSOR_VALID_UV;
    sensor_data->uvi_x100 = ltr390.uvi_x100;
    note_result(FAULT_COMP_LTR390, true);
  }
  else
  {
    note_result(FAULT_COMP_LTR390, false);
  }

  /* 5. Read SCD41 */
  if (SCD41_ENABLED && (sensor_flags & SENSOR_FLAG_CO2))
  {
    uint16_t scd41_co2_ppm = 0U;
    if (SCD41_ReadCo2SingleShot(&scd41_co2_ppm, NULL, NULL) == SCD41_STATUS_OK)
    {
      sensor_data->valid |= SENSOR_VALID_CO2;
      sensor_data->co2_ppm = scd41_co2_ppm;
    }
    note_result(FAULT_COMP_SCD41, (sensor_data->valid & SENSOR_VALID_CO2) != 0U);
  }

  /* 6. Read SPS30 */
  if (sensor_flags & SENSOR_FLAG_SPS30)
  {
    SPS30_Data_t sps30_data;
    if (SPS30_ReadMeasurement(&sps30_data) == SPS30_STATUS_OK)
    {
      sensor_data->valid |= SENSOR_VALID_PM;
      sensor_data->pm1_0    = sps30_data.mc_1_0;   /* 0.1 ug/m3 */
      sensor_data->pm2_5    = sps30_data.mc_2_5;
      sensor_data->pm4_0    = sps30_data.mc_4_0;
      sensor_data->pm10_0   = sps30_data.mc_10_0;
      sensor_data->nc_0_5   = sps30_data.nc_0_5;   /* 0.1 #/cm3 */
      sensor_data->nc_1_0   = sps30_data.nc_1_0;
      sensor_data->nc_2_5   = sps30_data.nc_2_5;
      sensor_data->nc_4_0   = sps30_data.nc_4_0;
      sensor_data->nc_10_0  = sps30_data.nc_10_0;
      sensor_data->typ_size = sps30_data.typ_size; /* nm */
    }
    note_result(FAULT_COMP_SPS30, (sensor_data->valid & SENSOR_VALID_PM) != 0U);
  }

  return 0;
  /* USER CODE END EnvSensors_Read */
}

int32_t EnvSensors_Init(void)
{
  /* USER CODE BEGIN EnvSensors_Init */
  /* A part missing here is reported once as 0x01xx on fPort 99 and as
   * resolved (0x81xx) when a later read succeeds. */
  bool found = SHT45_Init() == SHT45_OK;
  Fault_ComponentInit(FAULT_COMP_SHT45, found, ADDR_SHT45);
  if (!found) APP_LOG(TS_OFF, VLEVEL_M, "SHT45 not found\r\n");

  found = BMP390_Init() == BMP390_OK;
  Fault_ComponentInit(FAULT_COMP_BMP390, found, ADDR_BMP390);
  if (!found) APP_LOG(TS_OFF, VLEVEL_M, "BMP390 not found\r\n");

  found = LTR390_Init() == LTR390_OK;
  Fault_ComponentInit(FAULT_COMP_LTR390, found, ADDR_LTR390);
  if (!found) APP_LOG(TS_OFF, VLEVEL_M, "LTR390 not found\r\n");

  found = MAX17048_Init() == MAX17048_OK;
  Fault_ComponentInit(FAULT_COMP_MAX17048, found, ADDR_MAX17048);
  if (!found) APP_LOG(TS_OFF, VLEVEL_M, "MAX17048 not found\r\n");

  if (SCD41_ENABLED)
  {
    found = SCD41_Init() == SCD41_STATUS_OK;
    Fault_ComponentInit(FAULT_COMP_SCD41, found, ADDR_SCD41);
    if (!found) APP_LOG(TS_OFF, VLEVEL_M, "SCD41 not found\r\n");
  }

  found = SPS30_Init() == SPS30_STATUS_OK;
  Fault_ComponentInit(FAULT_COMP_SPS30, found, SPS30_I2C_ADDR);
  if (!found) APP_LOG(TS_OFF, VLEVEL_M, "SPS30 not found\r\n");

  found = INA226_Init() == INA226_OK;
  Fault_ComponentInit(FAULT_COMP_INA226, found, INA226_I2C_ADDR_7B);
  if (!found) APP_LOG(TS_OFF, VLEVEL_M, "INA226 not found\r\n");

  return 0;
  /* USER CODE END EnvSensors_Init */
}

/* USER CODE BEGIN EF */
int32_t EnvSensors_StartPreMeasurement(uint8_t sensor_flags)
{
  if(SCD41_ENABLED && (sensor_flags & SENSOR_FLAG_CO2))
  {
    return SCD41_StartCo2SingleShot();
  }
  if(sensor_flags & SENSOR_FLAG_SPS30)
  {
    /* The wake-up handshake is unreliable by design, so it must not gate the
     * measurement: skipping the start leaves the sensor idle, and reading an
     * idle SPS30 yields CRC-valid zeroes that pass as a measurement. The start
     * command is a real command and is the verdict on sensor health. */
    (void)SPS30_WakeUp();
    return SPS30_StartMeasurement();
  }
  return -1;
}

int32_t EnvSensors_RestartPreMeasurement(uint8_t sensor_flags)
{
  if(SCD41_ENABLED && (sensor_flags & SENSOR_FLAG_CO2))
  {
    return SCD41_DiscardAndRestartCo2SingleShot();
  }
  return -1;
}
/* USER CODE END EF */

/* Private Functions Definition -----------------------------------------------*/
/* USER CODE BEGIN PrFD */
/* The HAL error bits are those of the failed transfer, read before any bus
 * recovery resets the handle. */
static void note_result(uint8_t comp, bool ok)
{
  Fault_ComponentResult(comp, ok, ok ? 0U : HAL_I2C_GetError(&hi2c2));
}
/* USER CODE END PrFD */


int32_t EnvSensors_Sleep(void)
{
  int32_t status = SPS30_StopMeasurement();
  if (SPS30_Sleep() != SPS30_STATUS_OK) status = -1;
  if (SCD41_ENABLED && SCD41_Sleep() != SCD41_STATUS_OK) status = -1;
  return status;
}
