/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.c
  * @author  MCD Application Team
  * @brief   Application of the LRWAN Middleware
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "Region.h" /* Needed for LORAWAN_DEFAULT_DATA_RATE */
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "lora_app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "stm32_lpm.h"
#include "adc_if.h"
#include "sys_conf.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/**
  * @brief LoRa State Machine states
  */
typedef enum TxEventType_e
{
  /**
    * @brief Appdata Transmission issue based on timer every TxDutyCycleTime
    */
  TX_ON_TIMER,
  /**
    * @brief Appdata Transmission external event plugged on OnSendEvent( )
    */
  TX_ON_EVENT
  /* USER CODE BEGIN TxEventType_t */

  /* USER CODE END TxEventType_t */
} TxEventType_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define TX_SENSOR_CYCLE_LENGTH      3U
#define TX_SENSOR_CYCLE_FULL_INDEX  2U
#define TX_PREWAKE_GUARD_MS         2000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  LoRa End Node send request
  */
static void SendTxData(void);

/**
  * @brief  TX timer callback function
  * @param  context ptr of timer context
  */
static void OnTxTimerEvent(void *context);

/**
  * @brief  join event callback function
  * @param  joinParams status of join
  */
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);

/**
  * @brief  tx event callback function
  * @param  params status of last Tx
  */
static void OnTxData(LmHandlerTxParams_t *params);

/**
  * @brief callback when LoRa application has received a frame
  * @param appData data received in the last Rx
  * @param params status of last Rx
  */
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/*!
 * Will be called each time a Radio IRQ is handled by the MAC layer
 *
 */
static void OnMacProcessNotify(void);

/* USER CODE BEGIN PFP */

/**
  * @brief  LED Tx timer callback function
  * @param  context ptr of LED context
  */
static void OnTxTimerLedEvent(void *context);

/**
  * @brief  LED Rx timer callback function
  * @param  context ptr of LED context
  */
static void OnRxTimerLedEvent(void *context);

/**
  * @brief  LED Join timer callback function
  * @param  context ptr of LED context
  */
static void OnJoinTimerLedEvent(void *context);

/**
  * @brief  SCD41 pre-wake callback
  * @param  context ptr of timer context
  */
static void OnPreWakeTimerEvent(void *context);

/**
  * @brief  Arm/disarm pre-wake for the next scheduled TX slot
  */
static void SchedulePreWakeForCurrentCycle(void);

/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

/**
  * @brief LoRaWAN handler Callbacks
  */
static LmHandlerCallbacks_t LmHandlerCallbacks =
{
  .GetBatteryLevel =           GetBatteryLevel,
  .GetTemperature =            GetTemperatureLevel,
  .GetUniqueId =               GetUniqueId,
  .GetDevAddr =                GetDevAddr,
  .OnMacProcess =              OnMacProcessNotify,
  .OnJoinRequest =             OnJoinRequest,
  .OnTxData =                  OnTxData,
  .OnRxData =                  OnRxData
};

/**
  * @brief LoRaWAN handler parameters
  */
static LmHandlerParams_t LmHandlerParams =
{
  .ActiveRegion =             ACTIVE_REGION,
  .DefaultClass =             LORAWAN_DEFAULT_CLASS,
  .AdrEnable =                LORAWAN_ADR_STATE,
  .TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
  .PingPeriodicity =          LORAWAN_DEFAULT_PING_SLOT_PERIODICITY
};

/**
  * @brief Type of Event to generate application Tx
  */
static TxEventType_t EventType = TX_ON_TIMER;

/**
  * @brief Timer to handle the application Tx
  */
static UTIL_TIMER_Object_t TxTimer;

/* USER CODE BEGIN PV */
/**
  * @brief User application buffer
  */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];

/**
  * @brief User application data structure
  */
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };

/**
  * @brief Specifies the state of the application LED
  */
static uint8_t AppLedStateOn = RESET;

/**
  * @brief Timer to handle the application Tx Led to toggle
  */
static UTIL_TIMER_Object_t TxLedTimer;

/**
  * @brief Timer to handle the application Rx Led to toggle
  */
static UTIL_TIMER_Object_t RxLedTimer;

/**
  * @brief Timer to handle the application Join Led to toggle
  */
static UTIL_TIMER_Object_t JoinLedTimer;

/**
  * @brief Tx cycle index: 0,1 -> short keepalive payload (zeros) on FPort 2, 2 -> full sensor payload on FPort 3
  */
static uint8_t TxSensorCycle = 2U;

/**
  * @brief Indicates that a CO2 single-shot measurement is in progress
  */
static uint8_t TxCo2Pending = 0U;

/**
  * @brief First CO2 sample after startup is discarded in power-cycled mode
  */
static uint8_t TxDiscardFirstCo2 = 1U;

/**
  * @brief Pre-wake timer: fires before TX to start SCD41 single-shot
  */
static UTIL_TIMER_Object_t PreWakeTimer;

/**
  * @brief Ensure TX scheduler is started once after join
  */
static uint8_t TxSchedulerStarted = 0U;

/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void LoRaWAN_Init(void)
{
  /* USER CODE BEGIN LoRaWAN_Init_1 */

  BSP_LED_Init(LED_RED);
#if defined (LPM_AWAKE_LED_ENABLED) && (LPM_AWAKE_LED_ENABLED == 1)
  BSP_LED_On(LED_RED);
#endif

  /* Get LoRa APP version*/
  APP_LOG(TS_OFF, VLEVEL_M, "APP_VERSION:        V%X.%X.%X\r\n",
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Get MW LoraWAN info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION: V%X.%X.%X\r\n",
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Get MW SubGhz_Phy info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:   V%X.%X.%X\r\n",
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB2_SHIFT));

  UTIL_TIMER_Create(&TxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&RxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&JoinLedTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);
  UTIL_TIMER_Create(&PreWakeTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnPreWakeTimerEvent, NULL);
  UTIL_TIMER_SetPeriod(&TxLedTimer, 500);
  UTIL_TIMER_SetPeriod(&RxLedTimer, 500);
  UTIL_TIMER_SetPeriod(&JoinLedTimer, 500);
  if (APP_TX_DUTYCYCLE > (SCD41_SINGLE_SHOT_WAIT_MS + TX_PREWAKE_GUARD_MS))
  {
    UTIL_TIMER_SetPeriod(&PreWakeTimer, APP_TX_DUTYCYCLE - SCD41_SINGLE_SHOT_WAIT_MS - TX_PREWAKE_GUARD_MS);
  }
  else
  {
    UTIL_TIMER_SetPeriod(&PreWakeTimer, 1000U);
  }

  /* USER CODE BEGIN LoRaWAN_Init_debug_delay */
  /* Debug delay: 10 seconds after power-on to allow serial monitor to connect */
  APP_LOG(TS_OFF, VLEVEL_M, "Debug delay: waiting 10s...\r\n");
  HAL_Delay(10000);
  APP_LOG(TS_OFF, VLEVEL_M, "Starting LoRaWAN...\r\n");
  /* USER CODE END LoRaWAN_Init_debug_delay */

  /* Configure Low Power mode: STOP2, no Standby/Off */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_ENABLE);
  UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);

  /* USER CODE END LoRaWAN_Init_1 */

  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LmHandlerProcess), UTIL_SEQ_RFU, LmHandlerProcess);
  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), UTIL_SEQ_RFU, SendTxData);
  /* Init Info table used by LmHandler*/
  LoraInfo_Init();

  /* Init the Lora Stack*/
  LmHandlerInit(&LmHandlerCallbacks);

  LmHandlerConfigure(&LmHandlerParams);

  /* USER CODE BEGIN LoRaWAN_Init_2 */
  UTIL_TIMER_Start(&JoinLedTimer);

  /* USER CODE END LoRaWAN_Init_2 */

  LmHandlerJoin(ActivationType);

  if (EventType == TX_ON_TIMER)
  {
    /* send every fixed period, independent from pre-wake completion */
    UTIL_TIMER_Create(&TxTimer,  0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnTxTimerEvent, NULL);
    UTIL_TIMER_SetPeriod(&TxTimer,  APP_TX_DUTYCYCLE);
  }
  else
  {
    /* USER CODE BEGIN LoRaWAN_Init_3 */

    /* send every time button is pushed */
    BSP_PB_Init(BUTTON_SW1, BUTTON_MODE_EXTI);
    /* USER CODE END LoRaWAN_Init_3 */
  }

  /* USER CODE BEGIN LoRaWAN_Init_Last */

  /* USER CODE END LoRaWAN_Init_Last */
}

/* USER CODE BEGIN PB_Callbacks */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case  BUTTON_SW1_PIN:
      /* Note: when "EventType == TX_ON_TIMER" this GPIO is not initialized */
      UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
      break;
    default:
      break;
  }
}

/* USER CODE END PB_Callbacks */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
  /* USER CODE BEGIN OnRxData_1 */
  if ((appData != NULL) || (params != NULL))
  {

    UTIL_TIMER_Start(&RxLedTimer);

    static const char *slotStrings[] = { "1", "2", "C", "C Multicast", "B Ping-Slot", "B Multicast Ping-Slot" };

    APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Indication ==========\r\n");
    APP_LOG(TS_OFF, VLEVEL_H, "###### D/L FRAME:%04d | SLOT:%s | PORT:%d | DR:%d | RSSI:%d | SNR:%d\r\n",
            params->DownlinkCounter, slotStrings[params->RxSlot], appData->Port, params->Datarate, params->Rssi, params->Snr);
    switch (appData->Port)
    {
      case LORAWAN_SWITCH_CLASS_PORT:
        /*this port switches the class*/
        if (appData->BufferSize == 1)
        {
          switch (appData->Buffer[0])
          {
            case 0:
            {
              LmHandlerRequestClass(CLASS_A);
              break;
            }
            case 1:
            {
              LmHandlerRequestClass(CLASS_B);
              break;
            }
            case 2:
            {
              LmHandlerRequestClass(CLASS_C);
              break;
            }
            default:
              break;
          }
        }
        break;
      case LORAWAN_USER_APP_PORT:
        if (appData->BufferSize == 1)
        {
          AppLedStateOn = appData->Buffer[0] & 0x01;
#if !defined (LPM_AWAKE_LED_ENABLED) || (LPM_AWAKE_LED_ENABLED == 0)
          if (AppLedStateOn == RESET)
          {
            APP_LOG(TS_OFF, VLEVEL_H,   "LED OFF\r\n");
            BSP_LED_Off(LED_RED) ;
          }
          else
          {
            APP_LOG(TS_OFF, VLEVEL_H, "LED ON\r\n");
            BSP_LED_On(LED_RED) ;
          }
#endif
        }
        break;

      default:

        break;
    }
  }
  /* USER CODE END OnRxData_1 */
}

static void SendTxData(void)
{
  /* USER CODE BEGIN SendTxData_1 */
  APP_LOG(TS_OFF, VLEVEL_M, "SendTxData func:\r\n");
  sensor_t sensor_data = {0};
  UTIL_TIMER_Time_t nextTxIn = 0;

  uint16_t humidity = 0;
  uint16_t co2 = 0;
  uint32_t i = 0;
  uint16_t temperatureRaw = 0;
  uint8_t sendFullPayload = (TxSensorCycle == TX_SENSOR_CYCLE_FULL_INDEX) ? 1U : 0U;

  if (sendFullPayload == 0U)
  {
    /* SHORT payload path: keepalive values (extension point for non-full sensors). */
    temperatureRaw = 0U;
    humidity = 0U;
  }

  else
  {
    /* FULL payload path: use sample prepared by pre-wake. */
    if (TxCo2Pending != 0U)
    {
      if (EnvSensors_ReadCo2SingleShot(&sensor_data) == 0)
      {
        co2 = sensor_data.co2;
        if (TxDiscardFirstCo2 != 0U)
        {
          co2 = 0U;
          TxDiscardFirstCo2 = 0U;
          APP_LOG(TS_OFF, VLEVEL_M, "SCD41 warm-up sample discarded\r\n");
        }
        temperatureRaw = (uint16_t)((sensor_data.temperature >= 0.0f) ?
                                    (sensor_data.temperature + 0.5f) :
                                    0.0f);
        humidity = (uint16_t)((sensor_data.humidity >= 0.0f) ?
                              (sensor_data.humidity * 10.0f + 0.5f) :
                              0.0f);
        APP_LOG(TS_OFF, VLEVEL_M, "SCD41 CO2: %u ppm\r\n", co2);
      }
      else
      {
        co2 = 0U;
        temperatureRaw = 0U;
        humidity = 0U;
        APP_LOG(TS_OFF, VLEVEL_M, "SCD41 single-shot read failed, sending CO2=0\r\n");
      }
    }
    else
    {
      co2 = 0U;
      temperatureRaw = 0U;
      humidity = 0U;
      APP_LOG(TS_OFF, VLEVEL_M, "PreWake missing, sending CO2=0\r\n");
    }

    TxCo2Pending = 0U;
  }

  AppData.Port = (sendFullPayload != 0U) ? 3 : 2;

  /* Short payload (FPort 2): Temp + Humi. Full payload (FPort 3): Temp + Humi + CO2 */
  AppData.Buffer[i++] = (uint8_t)((temperatureRaw >> 8) & 0xFF);
  AppData.Buffer[i++] = (uint8_t)(temperatureRaw & 0xFF);
  AppData.Buffer[i++] = (uint8_t)((humidity >> 8) & 0xFF);
  AppData.Buffer[i++] = (uint8_t)(humidity & 0xFF);

  if (sendFullPayload != 0U)
  {
    AppData.Buffer[i++] = (uint8_t)((co2 >> 8) & 0xFF);
    AppData.Buffer[i++] = (uint8_t)(co2 & 0xFF);
  }

  AppData.BufferSize = i;

  if (LORAMAC_HANDLER_SUCCESS == LmHandlerSend(&AppData, LORAWAN_DEFAULT_CONFIRMED_MSG_STATE, &nextTxIn, false))
  {
    APP_LOG(TS_ON, VLEVEL_L, "SEND REQUEST | FPORT:%d | SIZE:%d\r\n", AppData.Port, AppData.BufferSize);
    TxSensorCycle = (uint8_t)((TxSensorCycle + 1U) % TX_SENSOR_CYCLE_LENGTH);
  }
  else if (nextTxIn > 0)
  {
    APP_LOG(TS_ON, VLEVEL_L, "Next Tx in  : ~%d second(s)\r\n", (nextTxIn / 1000));
  }

  if (EventType == TX_ON_TIMER)
  {
    SchedulePreWakeForCurrentCycle();
  }

  /* USER CODE BEGIN SendTxData_sleep */
  /* After TX + RX windows: re-enable STOP2 sleep */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_ENABLE);
  UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
  /* USER CODE END SendTxData_sleep */

  /* USER CODE END SendTxData_1 */
}

static void OnTxTimerEvent(void *context)
{
  (void)context;
  /* USER CODE BEGIN OnTxTimerEvent_1 */

  /* USER CODE END OnTxTimerEvent_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
}

/* USER CODE BEGIN PrFD_LedEvents */
static void OnTxTimerLedEvent(void *context)
{

}

static void OnRxTimerLedEvent(void *context)
{

}

static void OnJoinTimerLedEvent(void *context)
{
#if !defined (LPM_AWAKE_LED_ENABLED) || (LPM_AWAKE_LED_ENABLED == 0)
  BSP_LED_Toggle(LED_RED) ;
#endif
}

static void OnPreWakeTimerEvent(void *context)
{
  (void)context;

  if (TxSensorCycle != TX_SENSOR_CYCLE_FULL_INDEX)
  {
    return;
  }

  if (TxCo2Pending != 0U)
  {
    return;
  }

  if (EnvSensors_StartCo2SingleShot() == 0)
  {
    TxCo2Pending = 1U;
    APP_LOG(TS_OFF, VLEVEL_M, "PreWake: SCD41 single-shot started\r\n");
  }
  else
  {
    APP_LOG(TS_OFF, VLEVEL_M, "PreWake: SCD41 start failed\r\n");
  }
}

static void SchedulePreWakeForCurrentCycle(void)
{
  UTIL_TIMER_Stop(&PreWakeTimer);

  if (TxSensorCycle == TX_SENSOR_CYCLE_FULL_INDEX)
  {
    UTIL_TIMER_Start(&PreWakeTimer);
  }
}

/* USER CODE END PrFD_LedEvents */

static void OnTxData(LmHandlerTxParams_t *params)
{
  /* USER CODE BEGIN OnTxData_1 */
  if ((params != NULL))
  {
    /* Process Tx event only if its a mcps response to prevent some internal events (mlme) */
    if (params->IsMcpsConfirm != 0)
    {

      UTIL_TIMER_Start(&TxLedTimer);

      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Confirm =============\r\n");
      APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:%04d | PORT:%d | DR:%d | PWR:%d", params->UplinkCounter,
              params->AppData.Port, params->Datarate, params->TxPower);

      APP_LOG(TS_OFF, VLEVEL_H, " | MSG TYPE:");
      if (params->MsgType == LORAMAC_HANDLER_CONFIRMED_MSG)
      {
        APP_LOG(TS_OFF, VLEVEL_H, "CONFIRMED [%s]\r\n", (params->AckReceived != 0) ? "ACK" : "NACK");
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_H, "UNCONFIRMED\r\n");
      }
    }
  }
  /* USER CODE END OnTxData_1 */
}

static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
  /* USER CODE BEGIN OnJoinRequest_1 */
  if (joinParams != NULL)
  {
    if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
    {
      UTIL_TIMER_Stop(&JoinLedTimer);
#if !defined (LPM_AWAKE_LED_ENABLED) || (LPM_AWAKE_LED_ENABLED == 0)
      BSP_LED_Off(LED_RED);
#endif

      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOINED = ");
      if (joinParams->Mode == ACTIVATION_TYPE_ABP)
      {
        APP_LOG(TS_OFF, VLEVEL_M, "ABP ======================\r\n");
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_M, "OTAA =====================\r\n");
      }

      /* USER CODE BEGIN OnJoinRequest_sleep */
      UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_ENABLE);
      UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
      APP_LOG(TS_OFF, VLEVEL_M, "###### Low power mode enabled\r\n");

      if ((EventType == TX_ON_TIMER) && (TxSchedulerStarted == 0U))
      {
        TxSchedulerStarted = 1U;
        UTIL_TIMER_Start(&TxTimer);
        SchedulePreWakeForCurrentCycle();
      }
      /* USER CODE END OnJoinRequest_sleep */
    }
    else
    {
      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOIN FAILED\r\n");
    }
  }
  /* USER CODE END OnJoinRequest_1 */
}

static void OnMacProcessNotify(void)
{
  /* USER CODE BEGIN OnMacProcessNotify_1 */

  /* USER CODE END OnMacProcessNotify_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LmHandlerProcess), CFG_SEQ_Prio_0);

  /* USER CODE BEGIN OnMacProcessNotify_2 */

  /* USER CODE END OnMacProcessNotify_2 */
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
