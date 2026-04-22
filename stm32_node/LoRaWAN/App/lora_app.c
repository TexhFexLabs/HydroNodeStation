/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.c
  * @author  MCD Application Team
  * @brief   Application of the LRWAN Middleware
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021-2025 STMicroelectronics.
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
#include "platform.h"
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "app_version.h"
#include "Commissioning.h"
#include "subghz_phy_version.h"
#include "smtc_modem_api.h"
#include "smtc_modem_utilities.h"
#include "smtc_modem_hal.h"
#include "smtc_modem_relay_api.h"
#include "adc_if.h"
#include "CayenneLpp.h"
#include "sys_sensors.h"
#include "flash_if.h"
#include "rng.h"
#include "lorawan_api.h"
#include "sps30.h"
#include "stm32_lpm.h"

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
/**
  * LEDs period value of the timer in ms
  */
#define LED_PERIOD_TIME 500

/**
  * Join switch period value of the timer in ms
  */
#define JOIN_TIME 2000

/**
  * Uplink time interval in Certification mode is 10s (recommended value)
  */
#define CERT_TX_DUTYCYCLE   10

/**
  * Stack id value (multistacks modem is not yet available)
  */
#define STACK_ID 0

/*---------------------------------------------------------------------------*/
/*                             LoRaWAN NVM configuration                     */
/*---------------------------------------------------------------------------*/
/**
  * @brief LoRaWAN NVM Flash address
  * @note last 2 sector of a 256kBytes device
  */
#define LORAWAN_NVM_BASE_ADDRESS        (0x0803F000UL)

#define SECURE_ELEMENT_CONTEXT_SIZE     0x2A0UL
#define MODEM_CONTEXT_SIZE              0x10UL
#define LORAWAN_CONTEXT_SIZE            0x28UL

#define ADDR_FLASH_LORAWAN_CONTEXT              LORAWAN_NVM_BASE_ADDRESS
#define ADDR_FLASH_MODEM_CONTEXT                (void *)(LORAWAN_NVM_BASE_ADDRESS + LORAWAN_CONTEXT_SIZE)
#define ADDR_FLASH_SECURE_ELEMENT_CONTEXT       (void *)(LORAWAN_NVM_BASE_ADDRESS + LORAWAN_CONTEXT_SIZE + MODEM_CONTEXT_SIZE)

/* USER CODE BEGIN PD */
/**
  * @brief SPS30 Manual Fan cleaning interval in hours
  */
#define SPS30_FAN_CLEAN_INTERVAL_HOURS  120U

/**
  * @brief RX command port and codes
  */
#define RX_CMD_PORT                      LORAWAN_USER_APP_PORT
#define RX_CMD_TRIGGER_SPS30_CLEANING    0x11U
#define RX_CMD_SOFTWARE_RESET            0xFFU

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/

/*!
 * @brief Stringify constants
 */
#define xstr( a ) str( a )
#define str( a ) #a

/*!
 * @brief Helper macro that returned a human-friendly message if a command does not return SMTC_MODEM_RC_OK
 *
 * @remark The macro is implemented to be used with functions returning a @ref smtc_modem_return_code_t
 *
 * @param[in] rc  Return code
 */

#define ASSERT_SMTC_MODEM_RC( rc_func )                                                     \
  do                                                                                        \
  {                                                                                         \
    smtc_modem_return_code_t rc = rc_func;                                                  \
    if( rc == SMTC_MODEM_RC_NOT_INIT )                                                      \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_H,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__,   \
              xstr( SMTC_MODEM_RC_NOT_INIT ) );                             \
    }                                                                                       \
    else if( rc == SMTC_MODEM_RC_INVALID )                                                  \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_H,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__,   \
              xstr( SMTC_MODEM_RC_INVALID ) );                              \
    }                                                                                       \
    else if( rc == SMTC_MODEM_RC_BUSY )                                                     \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_H,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__,   \
              xstr( SMTC_MODEM_RC_BUSY ) );                                 \
    }                                                                                       \
    else if( rc == SMTC_MODEM_RC_FAIL )                                                     \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_H,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__,   \
              xstr( SMTC_MODEM_RC_FAIL ) );                                 \
    }                                                                                       \
    else if( rc == SMTC_MODEM_RC_NO_TIME )                                                  \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_L,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__, \
              xstr( SMTC_MODEM_RC_NO_TIME ) );                            \
    }                                                                                       \
    else if( rc == SMTC_MODEM_RC_INVALID_STACK_ID )                                         \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_H,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__,   \
              xstr( SMTC_MODEM_RC_INVALID_STACK_ID ) );                     \
    }                                                                                       \
    else if( rc == SMTC_MODEM_RC_NO_EVENT )                                                 \
    {                                                                                       \
      APP_LOG(TS_OFF, VLEVEL_M,  "In %s - %s (line %d): %s\r\n", __FILE__, __func__, __LINE__,    \
              xstr( SMTC_MODEM_RC_NO_EVENT ) );                              \
    }                                                                                       \
  } while( 0 )

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  LoRa End Node send request
  */
static void SendTxData(uint8_t port);
static void processRxData(const uint8_t *payload, uint8_t size, const smtc_modem_dl_metadata_t *metadata);

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
/**
  * @brief  Sleep timer callback function
  * @param  context ptr
  */
static void OnSleepTimerEvent(void *context);
#endif /* (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0) */

/**
  * @brief User callback for event
  *
  *  This callback is called every time an event ( see smtc_modem_event_t ) appears in the modem.
  *  Several events may have to be read from the modem when this callback is called.
  */
static void EventCallback(void);

/*!
 * Restore the NVM Data context from the Flash
 *
* @param [in]  ctx_type   Type of modem context that need to be restored
* @param [in]  offset     Memory offset after ctx_type address
* @param [out] buffer     Buffer pointer to write to
* @param [in]  size       Buffer size to read in bytes
*/
static void RestoreContext(const modem_context_type_t ctx_type, uint32_t offset, uint8_t *buffer, const uint32_t size);
/*!
 * Store the NVM Data context to the Flash
 *
* @param [in] ctx_type   Type of modem context that need to be saved
* @param [in] offset     Memory offset after ctx_type address
* @param [in] buffer     Buffer pointer to write from
* @param [in] size       Buffer size to write in bytes
*/
static void StoreContext(const modem_context_type_t ctx_type, uint32_t offset, const uint8_t *buffer,
                         const uint32_t size);
/*!
 * Get Random value using the RNG module
 *
 * \retval value  Return the random value
 */
static uint32_t GetRandomValue(void);

/*!
 * Will be called to reset the system
 * \note Compliance test protocol callbacks used when TS001-1.0.4 + TS009 1.0.0 are defined
 */
static void SystemReset(void);

/* USER CODE BEGIN PFP */

static bool IsAllZero(const uint8_t *buffer, uint8_t size);

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
  * @brief  SCD41 pre-measurement timer callback function
  * @param  context ptr
  */
static void OnScd41TimerEvent(void *context);

/**
  * @brief  SPS30 pre-measurement timer callback function
  * @param  context ptr
  */
static void OnSps30TimerEvent(void *context);

/**
  * @brief  SPS30 fan cleanup timer callback function
  * @param  context ptr
  */
static void OnSps30CleanupTimerEvent(void *context);

/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/

/**
  * @brief LoRaWAN User credentials
  */
static uint8_t user_dev_eui[8]      = FORMAT32_KEY(LORAWAN_DEVICE_EUI);
static uint8_t user_join_eui[8]     = FORMAT32_KEY(LORAWAN_JOIN_EUI);
static uint8_t user_gen_app_key[16] = FORMAT_KEY(LORAWAN_GEN_APP_KEY);
static uint8_t user_app_key[16]     = FORMAT_KEY(LORAWAN_APP_KEY);
/**
  * @brief  Buffer for rx payload
  */
static uint8_t                  rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 };

/**
  * @brief  Size of the payload in the rx_payload buffer
  */
static uint8_t                  rx_payload_size = 0;

/**
  * @brief  Metadata of downlink
  */
static smtc_modem_dl_metadata_t rx_metadata     = { 0 };

/**
  * @brief  Remaining downlink payload in modem
  */
static uint8_t                  rx_remaining    = 0;

/**
 * @brief TX counter
 */
static uint8_t                  tx_counter    = 0U;

/**
 * @brief Last SPS30 fan cleaning timestamp
 */
static uint32_t                 last_sps30_clean_timestamp = 0U;

/**
  * @brief  Flag for button status
  */
static volatile bool user_button_is_press = false;

/**
  * @brief LoRaWAN Certification Mode
  */
static bool CertMode = LORAWAN_CERTIFICATION_MODE;

/**
  * @brief Type of Event to generate application Tx
  */
static TxEventType_t EventType = TX_ON_TIMER;

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
/**
  * @brief Timer to handle the sleep time
  */
static UTIL_TIMER_Object_t SleepTimer;
#endif /* (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0) */

/**
  * Temp buffer to store a FLASH page in RAM when partial replacement is needed
  */
static uint8_t FLASH_RAM_buffer[FLASH_IF_BUFFER_SIZE];

/**
  * @brief Handler Callbacks
  */
static Callbacks_t Callbacks =
{
  .EventCallback =                EventCallback,
  .RestoreContext =               RestoreContext,
  .StoreContext =                 StoreContext,
  .GetRandomValue =               GetRandomValue,
  .GetBatteryLevel =              GetBatteryLevel,
  .GetTemperatureLevel =          GetTemperatureLevel,
  .SystemReset =                  SystemReset,
};

/* USER CODE BEGIN PV */
/**
  * @brief User application buffer
  */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];

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
  * @brief Timer to trigger SCD41 pre-measurement
  */
static UTIL_TIMER_Object_t Scd41Timer;

/**
  * @brief Timer to trigger SPS30 pre-measurement
  */
static UTIL_TIMER_Object_t Sps30Timer;

/**
  * @brief Timer to stop SPS30 fan cleaning
  */
static UTIL_TIMER_Object_t Sps30CleanupTimer;

/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void LoRaWAN_Init(void)
{
  /* USER CODE BEGIN LoRaWAN_Init_LV */
  lr1mac_version_t lorawan_version;
  lr1mac_version_t rp_version;

  /* USER CODE END LoRaWAN_Init_LV */

  /* USER CODE BEGIN LoRaWAN_Init_1 */

  APP_LOG(TS_OFF, VLEVEL_M, "LoRaWAN End Node LBM\r\n");
  /* Get LoRaWAN APP version*/
  APP_LOG(TS_OFF, VLEVEL_M, "APPLICATION_VERSION: V%X.%X.%X\r\n",
          (uint8_t)(APP_VERSION_MAIN),
          (uint8_t)(APP_VERSION_SUB1),
          (uint8_t)(APP_VERSION_SUB2));

  /* Get MW LoRaWAN info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION:  V%X.%X.%X\r\n",
          (uint8_t)(LORAWAN_VERSION_MAIN),
          (uint8_t)(LORAWAN_VERSION_SUB1),
          (uint8_t)(LORAWAN_VERSION_SUB2));

  /* Get MW SubGhz_Phy info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:    V%X.%X.%X\r\n",
          (uint8_t)(SUBGHZ_PHY_VERSION_MAIN),
          (uint8_t)(SUBGHZ_PHY_VERSION_SUB1),
          (uint8_t)(SUBGHZ_PHY_VERSION_SUB2));

  /* Get LoRaWAN Link Layer info */
  memset(&lorawan_version, 0, sizeof(lr1mac_version_t));
  lorawan_version = lorawan_api_get_spec_version(STACK_ID);

  APP_LOG(TS_OFF, VLEVEL_M, "L2_SPEC_VERSION:     V%X.%X.%X\r\n",
          (uint8_t)(lorawan_version.major),
          (uint8_t)(lorawan_version.minor),
          (uint8_t)(lorawan_version.patch));

  /* Get LoRaWAN Regional Parameters info */
  memset(&rp_version, 0, sizeof(lr1mac_version_t));
  rp_version = lorawan_api_get_regional_parameters_version(STACK_ID);
  APP_LOG(TS_OFF, VLEVEL_M, "RP_SPEC_VERSION:     V%X-%X.%X.%X\r\n",
          (uint8_t)(rp_version.revision),
          (uint8_t)(rp_version.major),
          (uint8_t)(rp_version.minor),
          (uint8_t)(rp_version.patch));

  UTIL_TIMER_Create(&TxLedTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&RxLedTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&JoinLedTimer, LED_PERIOD_TIME, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);
  UTIL_TIMER_Create(&Scd41Timer, SCD41_PRE_MEASUREMENT_TIME_MS, UTIL_TIMER_ONESHOT, OnScd41TimerEvent, NULL);
  UTIL_TIMER_Create(&Sps30Timer, SPS30_PRE_MEASUREMENT_TIME_MS, UTIL_TIMER_ONESHOT, OnSps30TimerEvent, NULL);
  UTIL_TIMER_Create(&Sps30CleanupTimer, SPS30_CLEANING_DURATION_MS, UTIL_TIMER_ONESHOT, OnSps30CleanupTimerEvent, NULL);

  /* USER CODE END LoRaWAN_Init_1 */

  if (FLASH_IF_Init(FLASH_RAM_buffer) != FLASH_IF_OK)
  {
    Error_Handler();
  }

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
  UTIL_TIMER_Create(&SleepTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnSleepTimerEvent, NULL);
#endif /* (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0) */
  /* Init the Lora Stack*/
  /* Init the modem and use EventCallback as event callback, please note that the callback will be */
  /* called immediately after the first call to smtc_modem_run_engine because of the reset detection */
  smtc_modem_init(&Callbacks);

  /* Certification mode is disabled by default. It can be enabled setting LORAWAN_CERTIFICATION_MODE to true */
  smtc_modem_set_certification_mode(STACK_ID, CertMode);

  /* BSP crystal accurrancy could be set to a different value. By default it is 10. */
  smtc_modem_set_crystal_error_ppm(BSP_CRYSTAL_ERROR);

  /* USER CODE BEGIN LoRaWAN_Init_Last */
  UTIL_TIMER_Start(&JoinLedTimer);

  EnvSensors_Init();
  
  /* Initial fan clean check if battery is good */
  sensor_t init_sensor_data;
  EnvSensors_Read(&init_sensor_data, SENSOR_FLAG_ONLY_BATTERY); // Just read battery
  if (init_sensor_data.battery_voltage > 4000)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "Initial SPS30 fan cleaning (VBat=%u mV)\r\n", (unsigned)init_sensor_data.battery_voltage);
    if ((SPS30_AcquireBus() == SPS30_STATUS_OK) && (SPS30_WakeUp() == SPS30_STATUS_OK))
    {
       (void)SPS30_StartMeasurement();
       HAL_Delay(50);
       if (SPS30_StartFanCleaning() == SPS30_STATUS_OK)
       {
         UTIL_TIMER_Start(&Sps30CleanupTimer);
       }
       else
       {
         APP_LOG(TS_OFF, VLEVEL_M, "Initial SPS30 fan cleaning start failed\r\n");
         (void)SPS30_StopMeasurement();
         (void)SPS30_Sleep();
       }
        last_sps30_clean_timestamp = SysTimeGet().Seconds;
    }
    else
    {
      APP_LOG(TS_OFF, VLEVEL_M, "Initial SPS30 bus acquire/wakeup failed\r\n");
    }
    (void)SPS30_ReleaseBus();
  }

  APP_LOG(TS_OFF, VLEVEL_M, "Sensors initialized\r\n");
  /* USER CODE END LoRaWAN_Init_Last */
}

void LoRaWAN_Process(void)
{
  uint32_t sleep_time_ms = 0;
  /* Check button */
  if (user_button_is_press == true)
  {
    user_button_is_press = false;

    smtc_modem_status_mask_t status_mask = 0;
    smtc_modem_get_status(STACK_ID, &status_mask);
    /* Check if the device has already joined a network */
    if ((status_mask & SMTC_MODEM_STATUS_JOINED) == SMTC_MODEM_STATUS_JOINED)
    {
      /* Send packet */
      SendTxData(LORAWAN_USER_APP_PORT);
    }
  }

  /* Modem process launch */
  sleep_time_ms = smtc_modem_run_engine();

  /* Atomically check sleep conditions (button was not pressed and no modem flags pending) */

  if ((user_button_is_press == false) && (smtc_modem_is_irq_flag_pending() == false))
  {
    if (sleep_time_ms > 0)
    {
#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
      UTIL_TIMER_SetPeriod(&SleepTimer, sleep_time_ms);
      UTIL_TIMER_Start(&SleepTimer);
      UTIL_LPM_EnterLowPower();
#endif
    }
  }

}

static void SystemReset(void)
{
  /* USER CODE BEGIN SystemReset_1 */

  /* USER CODE END SystemReset_1 */
  __disable_irq();
  HAL_NVIC_SystemReset();   /* Restart system */
  /* USER CODE BEGIN SystemReset_Last */

  /* USER CODE END SystemReset_Last */
}

static uint32_t GetRandomValue(void)
{
  uint32_t rand_nb = 0;
  // Init and enable RNG
  hrng.Instance = RNG;
  hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;

  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }

  // Wait for data ready interrupt: 42+4 RNG clock cycles
  if (HAL_RNG_GenerateRandomNumber(&hrng, &rand_nb) != HAL_OK)
  {
    Error_Handler();
  }

  // Disable RNG
  HAL_RNG_DeInit(&hrng);

  return rand_nb;
}

static void RestoreContext(const modem_context_type_t ctx_type, uint32_t offset, uint8_t *buffer,
                           const uint32_t size)
{
  /* Offset is only used for fuota and store and forward purpose and for multistack features. To avoid ram consumption */
  /* the use of hal_flash_read_modify_write is only done in these cases */
  /* USER CODE BEGIN RestoreContext_1 */

  /* USER CODE END RestoreContext_1 */
  FLASH_IF_StatusTypedef ret_status = FLASH_IF_OK;
  switch (ctx_type)
  {
    case CONTEXT_MODEM:
      ret_status = FLASH_IF_Read(buffer, ADDR_FLASH_MODEM_CONTEXT, MODEM_CONTEXT_SIZE);
      break;
    case CONTEXT_LORAWAN_STACK:
      ret_status = FLASH_IF_Read(buffer, (void *)((uint32_t)(ADDR_FLASH_LORAWAN_CONTEXT + offset)), LORAWAN_CONTEXT_SIZE);
      break;
    case CONTEXT_SECURE_ELEMENT:
      ret_status = FLASH_IF_Read(buffer, ADDR_FLASH_SECURE_ELEMENT_CONTEXT, SECURE_ELEMENT_CONTEXT_SIZE);
      break;
    default:
      break;
  }
  if (ret_status != 0)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "restore ctx type %d, FLASH_IF return: %d\r\n", ctx_type, ret_status);
  }
  /* USER CODE BEGIN RestoreContext_Last */

  /* USER CODE END RestoreContext_Last */
}

static void StoreContext(const modem_context_type_t ctx_type, uint32_t offset, const uint8_t *buffer,
                         const uint32_t size)
{
  /* USER CODE BEGIN StoreContext_1 */

  /* USER CODE END StoreContext_1 */
  FLASH_IF_StatusTypedef ret_status = FLASH_IF_OK;
  /* Offset is only used for fuota and store and forward purpose and for multistack features. To avoid ram consumption
   * the use of hal_flash_read_modify_write is only done in these cases */
  switch (ctx_type)
  {
    case CONTEXT_MODEM:
    {
      ret_status = FLASH_IF_Write(ADDR_FLASH_MODEM_CONTEXT, (const void *)buffer, MODEM_CONTEXT_SIZE);
    }

    break;
    case CONTEXT_LORAWAN_STACK:
    {
      ret_status = FLASH_IF_Write((void *)ADDR_FLASH_LORAWAN_CONTEXT, (const void *)buffer, (uint32_t)LORAWAN_CONTEXT_SIZE);
    }
    break;
    case CONTEXT_SECURE_ELEMENT:
    {
      ret_status = FLASH_IF_Write(ADDR_FLASH_SECURE_ELEMENT_CONTEXT, (const void *)buffer, SECURE_ELEMENT_CONTEXT_SIZE);
    }
    break;

    default:
      break;
  }
  if (ret_status != 0)
  {
    APP_LOG(TS_OFF, VLEVEL_M, "store ctx type %d, FLASH_IF return: %d\r\n", ctx_type, ret_status);
  }
  /* USER CODE BEGIN StoreContext_Last */

  /* USER CODE END StoreContext_Last */
}

static void EventCallback(void)
{
  smtc_modem_event_t current_event;
  uint8_t            event_pending_count;
  uint8_t            stack_id = STACK_ID;
  smtc_modem_status_mask_t status_mask = 0;

  /* Continue to read modem event until all event has been processed */
  do
  {
    /* Read modem event */
    ASSERT_SMTC_MODEM_RC(smtc_modem_get_event(&current_event, &event_pending_count));

    switch (current_event.event_type)
    {
      case SMTC_MODEM_EVENT_RESET:
        APP_LOG(TS_OFF, VLEVEL_M, "Event received: RESET\r\n");

        GetUniqueId(user_dev_eui);

        /* Set user credentials */
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_deveui(stack_id, user_dev_eui));
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_joineui(stack_id, user_join_eui));
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_appkey(stack_id, user_gen_app_key));
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_nwkkey(stack_id, user_app_key));

        /* Set user region */
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_region(stack_id, ACTIVE_REGION));

        /* Print Security material */
        SecureElementPrintKeys(stack_id);
        CertMode = (smtc_modem_is_certification_port_disabled(STACK_ID)) ? 0 : CertMode;
        if (CertMode == false)
        {
          /* Schedule a Join LoRaWAN network */
          ASSERT_SMTC_MODEM_RC(smtc_modem_join_network(stack_id));
        }
        break;

      case SMTC_MODEM_EVENT_ALARM:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: ALARM\r\n");
        if (CertMode == true)
        {
          ASSERT_SMTC_MODEM_RC(smtc_modem_alarm_clear_timer());
        }
        else
        {
          /* Send periodical uplink */
          SendTxData(LORAWAN_USER_APP_PORT);
        }
        break;

      case SMTC_MODEM_EVENT_JOINED:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: JOINED\r\n");
        APP_LOG(TS_OFF, VLEVEL_H,  "Modem is now joined \r\n");
        /* USER CODE BEGIN EventCallback_1 */
        if (JoinLedTimer.IsRunning)
        {
          UTIL_TIMER_Stop(&JoinLedTimer);
          HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */
        }
        /* USER CODE END EventCallback_1 */
        if (CertMode == false)
        {
          /* Send first periodical uplink */
          SendTxData(LORAWAN_USER_APP_PORT);
        }
        break;

      case SMTC_MODEM_EVENT_TXDONE:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: TXDONE\r\n");
        APP_LOG(TS_OFF, VLEVEL_H,  "Transmission done \r\n");
        smtc_modem_get_status(STACK_ID, &status_mask);
        /* USER CODE BEGIN EventCallback_2 */
        /* Check if the device has already joined a network */
        if ((JoinLedTimer.IsRunning) && (status_mask & SMTC_MODEM_STATUS_JOINED) == SMTC_MODEM_STATUS_JOINED)
        {
          UTIL_TIMER_Stop(&JoinLedTimer);
          HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */
        }
        /* USER CODE END EventCallback_2 */
        break;

      case SMTC_MODEM_EVENT_DOWNDATA:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: DOWNDATA\r\n");
        /* USER CODE BEGIN EventCallback_3 */
        //HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET); /* LED_BLUE */
        UTIL_TIMER_Start(&RxLedTimer);
        /* USER CODE END EventCallback_3 */
        /* Get downlink data */
        ASSERT_SMTC_MODEM_RC(smtc_modem_get_downlink_data(rx_payload, &rx_payload_size, &rx_metadata, &rx_remaining));
        APP_LOG(TS_OFF, VLEVEL_M, "Data received on port %u\r\n", rx_metadata.fport);
        if (rx_payload_size == 0U)
        {
          APP_LOG(TS_ON, VLEVEL_M, "Ignoring empty downlink payload on port %u\r\n", rx_metadata.fport);
          break;
        }

        if (rx_remaining > 0U)
        {
          APP_LOG(TS_ON, VLEVEL_M, "Downlink payload truncated, remaining=%u\r\n", (unsigned)rx_remaining);
        }

        processRxData(rx_payload, rx_payload_size, &rx_metadata);
        /* APP_LOG(TS_OFF, VLEVEL_M, "Received payload", rx_payload, rx_payload_size ); */
        break;

      case SMTC_MODEM_EVENT_JOINFAIL:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: JOINFAIL\r\n");
        smtc_modem_get_status(STACK_ID, &status_mask);
        /* USER CODE BEGIN EventCallback_4 */
        /* Check if the device has already joined a network */
        if ((!JoinLedTimer.IsRunning) && (status_mask & SMTC_MODEM_STATUS_JOINED) != SMTC_MODEM_STATUS_JOINED)
        {
          UTIL_TIMER_Start(&JoinLedTimer);
        }
        /* USER CODE END EventCallback_4 */
        break;

      case SMTC_MODEM_EVENT_ALCSYNC_TIME:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: ALCSync service TIME\r\n");
        break;

      case SMTC_MODEM_EVENT_LINK_CHECK:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: LINK_CHECK\r\n");
        break;

      case SMTC_MODEM_EVENT_CLASS_B_PING_SLOT_INFO:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: CLASS_B_PING_SLOT_INFO\r\n");
        break;

      case SMTC_MODEM_EVENT_CLASS_B_STATUS:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: CLASS_B_STATUS\r\n");
        break;

      case SMTC_MODEM_EVENT_LORAWAN_MAC_TIME:
        APP_LOG(TS_OFF, VLEVEL_L,  "Event received: LORAWAN MAC TIME\r\n");
        break;

      case SMTC_MODEM_EVENT_LORAWAN_FUOTA_DONE:
      {
        bool status = current_event.event_data.fuota_status.successful;
        if (status == true)
        {
          APP_LOG(TS_OFF, VLEVEL_M,  "Event received: FUOTA SUCCESSFUL\r\n");
        }
        else
        {
          APP_LOG(TS_OFF, VLEVEL_L,  "Event received: FUOTA FAIL\r\n");
        }
        break;
      }

      case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_C:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: MULTICAST CLASS_C STOP\r\n");
        break;

      case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_B:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: MULTICAST CLASS_B STOP\r\n");
        break;

      case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_C:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: New MULTICAST CLASS_C \r\n");
        break;

      case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_B:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: New MULTICAST CLASS_B\r\n");
        break;

      case SMTC_MODEM_EVENT_FIRMWARE_MANAGEMENT:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: FIRMWARE_MANAGEMENT\r\n");
        if (current_event.event_data.fmp.status == SMTC_MODEM_EVENT_FMP_REBOOT_IMMEDIATELY)
        {
          HAL_NVIC_SystemReset();
        }
        break;

      case SMTC_MODEM_EVENT_STREAM_DONE:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: STREAM_DONE\r\n");
        break;

      case SMTC_MODEM_EVENT_UPLOAD_DONE:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: UPLOAD_DONE\r\n");
        break;

      case SMTC_MODEM_EVENT_DM_SET_CONF:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: DM_SET_CONF\r\n");
        break;

      case SMTC_MODEM_EVENT_MUTE:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: MUTE\r\n");
        break;
      case SMTC_MODEM_EVENT_REGIONAL_DUTY_CYCLE:
      {
        uint8_t duty_cycle_status = current_event.event_data.regional_duty_cycle.status;
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: DUTY_CYCLE busy %d\r\n", duty_cycle_status);
      }
      break;
      default:
        APP_LOG(TS_OFF, VLEVEL_M,  "Unknown event %u\r\n", current_event.event_type);
        break;
    }
  } while (event_pending_count > 0);
}

/* USER CODE BEGIN PB_Callbacks */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case  BUT1_Pin:
      /* Note: when "EventType == TX_ON_TIMER" this GPIO is not initialized */
      if (EventType == TX_ON_EVENT)
      {
        static uint32_t last_press_timestamp_ms = 0;

        /* Debounce the button press, avoid multiple triggers */
        if ((int32_t)(SysTimeToMs(SysTimeGet()) - last_press_timestamp_ms) > 500)
        {
          last_press_timestamp_ms = SysTimeToMs(SysTimeGet());
          user_button_is_press    = true;
        }
      }
      break;
    default:
      break;
  }
}


/* USER CODE END PB_Callbacks */

static void SendTxData(uint8_t port)
{
  /* USER CODE BEGIN SendTxData_1 */
  sensor_t sensor_data;
  uint8_t bufferSize = 0;

  /* Increment tx_counter */
  tx_counter++;
  if (tx_counter > 6U)
  {
    tx_counter = 1U;
  }

  /* Determine which sensors to read this cycle */
  uint8_t sensor_flags = 0U;
  if (tx_counter % 3U == 0U) { sensor_flags |= SENSOR_FLAG_CO2; }
  if (tx_counter % 6U == 0U) { sensor_flags |= SENSOR_FLAG_SPS30; }

  /* Read sensors */
  EnvSensors_Read(&sensor_data, sensor_flags);

  /* Log raw stored values (scaling noted in unit label). Receiver / human
   * reader converts: T/100 = degC, RH/100 = %, P/10 = hPa. */
  APP_LOG(TS_ON, VLEVEL_M, "Sensors: T=%d [0.01degC], RH=%u [0.01%%], P=%u [0.1hPa], VBAT=%u mV\r\n",
          (int)sensor_data.temperature,
          (unsigned)sensor_data.humidity,
          (unsigned)sensor_data.pressure,
          (unsigned)sensor_data.battery_voltage);

  if (sensor_flags & SENSOR_FLAG_CO2)
  {
    APP_LOG(TS_ON, VLEVEL_M, "SCD41: CO2=%u ppm\r\n", (unsigned int)sensor_data.co2_ppm);
  }

  if (sensor_flags & SENSOR_FLAG_SPS30)
  {
    APP_LOG(TS_ON, VLEVEL_M, "SPS30 MC: PM1.0=%u PM2.5=%u PM4.0=%u PM10.0=%u [0.1 ug/m3]\r\n",
            (unsigned)sensor_data.pm1_0,
            (unsigned)sensor_data.pm2_5,
            (unsigned)sensor_data.pm4_0,
            (unsigned)sensor_data.pm10_0);
    APP_LOG(TS_ON, VLEVEL_M, "SPS30 NC: PM0.5=%u PM1.0=%u PM2.5=%u PM4.0=%u PM10=%u [0.1 #/cm3]\r\n",
            (unsigned)sensor_data.nc_0_5,
            (unsigned)sensor_data.nc_1_0,
            (unsigned)sensor_data.nc_2_5,
            (unsigned)sensor_data.nc_4_0,
            (unsigned)sensor_data.nc_10_0);
    APP_LOG(TS_ON, VLEVEL_M, "SPS30 TypSize=%u [nm]\r\n", (unsigned)sensor_data.typ_size);
  }

  enum
  {
    LPP_CH_PRESSURE = 0,
    LPP_CH_TEMPERATURE,
    LPP_CH_HUMIDITY,
    LPP_CH_UV_RAW,
    LPP_CH_BATTERY_V,
    LPP_CH_CO2,
    LPP_CH_PM1_0,
    LPP_CH_PM2_5,
    LPP_CH_PM4_0,
    LPP_CH_PM10_0,
    LPP_CH_NC_0_5,
    LPP_CH_NC_1_0,
    LPP_CH_NC_2_5,
    LPP_CH_NC_4_0,
    LPP_CH_NC_10_0,
    LPP_CH_TYP_SIZE,
  };

  /* Send all scaled values as raw uint16 via Luminosity (2 bytes, big-endian).
   * Receiver must know scaling:
   *  PRESSURE    : uint16 * 0.1 hPa
   *  TEMPERATURE : int16  * 0.01 degC (bit-cast into uint16 on wire)
   *  HUMIDITY    : uint16 * 0.01 %
   *  UV_RAW      : uint16 (LTR390 20-bit counts, clamped)
   *  BATTERY_V   : uint16 mV
   *  CO2         : uint16 ppm
   *  PM*         : uint16 * 0.1 ug/m3 (MC)
   *  NC_*        : uint16 * 0.1 #/cm3
   *  TYP_SIZE    : uint16 nm (um*1000)
   */
  CayenneLppReset();
  CayenneLppAddLuminosity(LPP_CH_PRESSURE, sensor_data.pressure);
  CayenneLppAddLuminosity(LPP_CH_TEMPERATURE, (uint16_t)sensor_data.temperature);
  CayenneLppAddLuminosity(LPP_CH_HUMIDITY, sensor_data.humidity);
  CayenneLppAddLuminosity(LPP_CH_UV_RAW,
                          (uint16_t)(sensor_data.uv_raw > 0xFFFFU ? 0xFFFFU : sensor_data.uv_raw));
  CayenneLppAddLuminosity(LPP_CH_BATTERY_V, sensor_data.battery_voltage);

  if (sensor_flags & SENSOR_FLAG_CO2)
  {
    CayenneLppAddLuminosity(LPP_CH_CO2, sensor_data.co2_ppm);
  }

  if (sensor_flags & SENSOR_FLAG_SPS30)
  {
    uint32_t current_time_s = SysTimeGet().Seconds;
    bool cleaning_triggered = false;

    if (((current_time_s - last_sps30_clean_timestamp) > (SPS30_FAN_CLEAN_INTERVAL_HOURS * 3600U)) &&
        (sensor_data.battery_voltage > 4120))
    {
      APP_LOG(TS_OFF, VLEVEL_M, "Manual SPS30 fan cleaning (VBat=%u mV)\r\n", (unsigned)sensor_data.battery_voltage);
      if (SPS30_StartFanCleaning() == SPS30_STATUS_OK)
      {
        last_sps30_clean_timestamp = current_time_s;
        UTIL_TIMER_Start(&Sps30CleanupTimer);
        cleaning_triggered = true;
      } else
      {
        APP_LOG(TS_OFF, VLEVEL_M, "Failed to start SPS30 fan cleaning\r\n");
      }
    }

    if (!cleaning_triggered)
    {
      (void)SPS30_StopMeasurement();
      (void)SPS30_Sleep();
    }
    CayenneLppAddLuminosity(LPP_CH_PM1_0,    sensor_data.pm1_0);
    CayenneLppAddLuminosity(LPP_CH_PM2_5,    sensor_data.pm2_5);
    CayenneLppAddLuminosity(LPP_CH_PM4_0,    sensor_data.pm4_0);
    CayenneLppAddLuminosity(LPP_CH_PM10_0,   sensor_data.pm10_0);
    CayenneLppAddLuminosity(LPP_CH_NC_0_5,   sensor_data.nc_0_5);
    CayenneLppAddLuminosity(LPP_CH_NC_1_0,   sensor_data.nc_1_0);
    CayenneLppAddLuminosity(LPP_CH_NC_2_5,   sensor_data.nc_2_5);
    CayenneLppAddLuminosity(LPP_CH_NC_4_0,   sensor_data.nc_4_0);
    CayenneLppAddLuminosity(LPP_CH_NC_10_0,  sensor_data.nc_10_0);
    CayenneLppAddLuminosity(LPP_CH_TYP_SIZE, sensor_data.typ_size);
  }

  CayenneLppCopy(AppDataBuffer);
  bufferSize = CayenneLppGetSize();

  if (JoinLedTimer.IsRunning)
  {
    UTIL_TIMER_Stop(&JoinLedTimer);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
  }

  ASSERT_SMTC_MODEM_RC(smtc_modem_request_uplink(STACK_ID, port, false, AppDataBuffer, bufferSize));

  if (EventType == TX_ON_TIMER)
  {
    smtc_modem_status_mask_t status_mask = 0;
    smtc_modem_get_status(STACK_ID, &status_mask);
    uint32_t dutycycle = (CertMode || ((status_mask & SMTC_MODEM_STATUS_JOINED) != SMTC_MODEM_STATUS_JOINED)) ? CERT_TX_DUTYCYCLE : APP_TX_DUTYCYCLE;

    if (sensor_data.battery_voltage < LOW_BATTERY_THRESHOLD_MV)
    {
      dutycycle = dutycycle * 2U; // Reduce TX frequency when battery is low
      APP_LOG(TS_ON, VLEVEL_M, "Low battery (%u mV), reducing TX frequency\r\n", (unsigned)sensor_data.battery_voltage);
    }
    if (sensor_data.battery_voltage < ULTRA_LOW_BATTERY_THRESHOLD_MV)
    {
      dutycycle = dutycycle * 6U;
      tx_counter = 0U;
      APP_LOG(TS_ON, VLEVEL_M, "ULTRA low battery (%u mV), reducing TX frequency only basic measurements\r\n", (unsigned)sensor_data.battery_voltage);
    }

    ASSERT_SMTC_MODEM_RC(smtc_modem_alarm_start_timer(dutycycle));

    /* Schedule pre-measurement for next TX if it will be a cycle for CO2 or SPS30 */
    uint8_t next_counter = (tx_counter % 6U) + 1U;
    if (next_counter % 3U == 0U)
    {
      UTIL_TIMER_SetPeriod(&Scd41Timer, (dutycycle * 1000U) - SCD41_PRE_MEASUREMENT_TIME_MS);
      UTIL_TIMER_Start(&Scd41Timer);
    }

    if (next_counter % 6U == 0U)
    {
      UTIL_TIMER_SetPeriod(&Sps30Timer, (dutycycle * 1000U) - SPS30_PRE_MEASUREMENT_TIME_MS);
      UTIL_TIMER_Start(&Sps30Timer);
    }
  }
  /* USER CODE END SendTxData_1 */
}

static void processRxData(const uint8_t *payload, uint8_t size, const smtc_modem_dl_metadata_t *metadata)
{
  if ((payload == NULL) || (metadata == NULL))
  {
    APP_LOG(TS_ON, VLEVEL_M, "Ignoring downlink: invalid arguments\r\n");
    return;
  }

  if (size == 0U)
  {
    APP_LOG(TS_ON, VLEVEL_M, "Ignoring downlink: payload size is zero\r\n");
    return;
  }

  if (metadata->fport != RX_CMD_PORT)
  {
    APP_LOG(TS_ON, VLEVEL_M, "Ignoring downlink command on unexpected port %u\r\n", metadata->fport);
    return;
  }

  const uint8_t command = payload[0];
  APP_LOG(TS_ON, VLEVEL_M, "Received command 0x%02X (size=%u, port=%u)\r\n",
          (unsigned)command,
          (unsigned)size,
          metadata->fport);

  switch (command)
  {
    case RX_CMD_TRIGGER_SPS30_CLEANING:
    {
      bool measurement_started = false;
      bool cleaning_started = false;

      APP_LOG(TS_ON, VLEVEL_M, "Message: Trigger SPS30 fan cleaning\r\n");

      if (SPS30_AcquireBus() != SPS30_STATUS_OK)
      {
        APP_LOG(TS_ON, VLEVEL_M, "SPS30 bus acquire failed\r\n");
        break;
      }

      if (SPS30_WakeUp() != SPS30_STATUS_OK)
      {
        (void)SPS30_ReleaseBus();
        APP_LOG(TS_ON, VLEVEL_M, "SPS30 wake-up failed\r\n");
        break;
      }

      if (SPS30_StartMeasurement() == SPS30_STATUS_OK)
      {
        measurement_started = true;
        HAL_Delay(50);
      }
      else
      {
        APP_LOG(TS_ON, VLEVEL_M, "SPS30 start measurement failed\r\n");
      }

      if (measurement_started && (SPS30_StartFanCleaning() == SPS30_STATUS_OK))
      {
        UTIL_TIMER_Start(&Sps30CleanupTimer);
        cleaning_started = true;
        last_sps30_clean_timestamp = SysTimeGet().Seconds;
        APP_LOG(TS_ON, VLEVEL_M, "SPS30 fan cleaning started\r\n");
      }
      else if (measurement_started)
      {
        APP_LOG(TS_ON, VLEVEL_M, "SPS30 fan cleaning start failed\r\n");
      }

      if ((cleaning_started == false) && (measurement_started == true))
      {
        (void)SPS30_StopMeasurement();
        (void)SPS30_Sleep();
      }

      (void)SPS30_ReleaseBus();

      break;
    }
    case RX_CMD_SOFTWARE_RESET:
      APP_LOG(TS_ON, VLEVEL_M, "Message: Trigger software reset\r\n");
      HAL_NVIC_SystemReset();
      break;
    default:
      APP_LOG(TS_ON, VLEVEL_M, "Message: Unknown command 0x%02X\r\n", (unsigned)command);
      break;
  }
}


/* USER CODE BEGIN PrFD_LedEvents */
static bool IsAllZero(const uint8_t *buffer, uint8_t size)
{
  for (uint8_t i = 0; i < size; i++)
  {
    if (buffer[i] != 0U)
    {
      return false;
    }
  }

  return true;
}

static void OnTxTimerLedEvent(void *context)
{
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET); /* LED_GREEN */
}

static void OnRxTimerLedEvent(void *context)
{
  //HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); /* LED_BLUE */
}

static void OnJoinTimerLedEvent(void *context)
{
  HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin); /* LED_RED */
}

static void OnScd41TimerEvent(void *context)
{
  APP_LOG(TS_ON, VLEVEL_M, "SCD41 pre-measurement started\r\n");
  EnvSensors_StartPreMeasurement(SENSOR_FLAG_CO2);
}

static void OnSps30TimerEvent(void *context)
{
  APP_LOG(TS_ON, VLEVEL_M, "SPS30 pre-measurement started\r\n");
  EnvSensors_StartPreMeasurement(SENSOR_FLAG_SPS30);
}

static void OnSps30CleanupTimerEvent(void *context)
{
  APP_LOG(TS_ON, VLEVEL_M, "SPS30 cleaning finished, going to sleep\r\n");
  (void)SPS30_StopMeasurement();
  (void)SPS30_Sleep();
}

/* USER CODE END PrFD_LedEvents */

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
static void OnSleepTimerEvent(void *context)
{
  /* USER CODE BEGIN OnSleepTimerEvent_1 */

  /* USER CODE END OnSleepTimerEvent_1 */
  APP_LOG(TS_ON, VLEVEL_H, "Sleep timer\r\n");

  /* USER CODE BEGIN OnSleepTimerEvent_Last */

  /* USER CODE END OnSleepTimerEvent_Last */
}
#endif /*(LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0) */

/* --- EOF ------------------------------------------------------------------ */
