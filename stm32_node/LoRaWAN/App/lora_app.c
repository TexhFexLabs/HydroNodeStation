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
#include "sys_sensors.h"
#include "rng.h"
#include "lorawan_api.h"
#include "sps30.h"
#include "stm32_lpm.h"

/* USER CODE BEGIN Includes */
#include "bq25185.h"
#include "power_policy.h"
#include "runtime_health.h"
#include "max17048.h"
#include "i2c.h"
#include "nvm_store.h"
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
/**
  * @brief Persistent application configuration layout (8 bytes, flash-stored).
  */
typedef struct
{
  uint32_t magic;
  uint32_t tx_dutycycle_s;
} app_config_t;
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
/* Legacy context sizes are validated at the NVM adapter boundary. */
#define SECURE_ELEMENT_CONTEXT_SIZE 0x2A0UL
#define MODEM_CONTEXT_SIZE          0x10UL
#define LORAWAN_CONTEXT_SIZE        0x28UL

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
#define RX_CMD_SET_TX_INTERVAL           0x10U
#define RX_CMD_SOFTWARE_RESET            0xFFU

#define TX_PORT_ENV_BASE                 2U
#define TX_PORT_ENV_EXTENDED             3U
#define TX_PORT_ENV_FULL                 4U

/* Application config shares the transactional NVM snapshots. */
#define APP_CONFIG_MAGIC                 (0x484E4331UL)   /* "HNC1" - HydroNode Config v1 */

/**
  * @brief BQ25185 charger safety-timer reset cadence in ms (independent CE-pulse timer).
  * @note  Must stay well below the charger's internal ~6 h safety timeout.
  */
#define BQ25185_SAFETY_RESET_INTERVAL_MS (2U * 60U * 60U * 1000U)

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
static void ProcessSensorEvents(void);
static void ServicePower(void);
static void ServiceJoin(void);
static void ReadBattery(void);
static void StopSensorTimers(void);
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

/* USER CODE BEGIN PFP

static bool IsAllZero(const uint8_t *buffer, uint8_t size);
 */
/**
  * @brief  LED Tx timer callback function
  * @param  context ptr of LED context
  
static void OnTxTimerLedEvent(void *context);
*/
/**
  * @brief  LED Rx timer callback function
  * @param  context ptr of LED context
static void OnRxTimerLedEvent(void *context);
 */
/**
  * @brief  LED Join timer callback function
  * @param  context ptr of LED context
  
static void OnJoinTimerLedEvent(void *context);
*/
/**
  * @brief  SCD41 pre-measurement timer callback function
  * @param  context ptr
  */
static void OnScd41TimerEvent(void *context);

/**
  * @brief  SCD41 restart timer callback function (power-cycled single shot, phase 2)
  * @param  context ptr
  */
static void OnScd41RestartTimerEvent(void *context);

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

/**
  * @brief  Charger CE timer callback: re-enables charging after the CE pulse
  * @param  context ptr
  */
static void OnChargerCeTimerEvent(void *context);

/**
  * @brief  Reset the BQ25185 safety timer: disable charging via CE,
  *         re-enable after BQ25185_CE_RESET_PULSE_MS (timer driven)
  */
static void ChargerSafetyTimerReset(void);

/**
  * @brief  Charger safety-timer periodic callback: pulses CE to reset the BQ25185 safety timer
  * @param  context ptr
  */
static void OnChargerSafetyTimerEvent(void *context);

/**
  * @brief  Load persistent app config from flash into RAM (falls back to defaults if invalid)
  */
static void AppConfig_Load(void);

/**
  * @brief  Persist the TX duty cycle to flash
  * @param  dutycycle_s  duty cycle in seconds (assumed already range-validated)
  * @retval true on success, false on flash write error
  */
static bool AppConfig_SaveTxDutycycle(uint32_t dutycycle_s);

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
/* NVM snapshots replace the legacy read/erase/rewrite page buffer. */

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

static UTIL_TIMER_Object_t TxLedTimer;
*/
/**
  * @brief Timer to handle the application Rx Led to toggle

static UTIL_TIMER_Object_t RxLedTimer;
*/
/**
  * @brief Timer to handle the application Join Led to toggle

static UTIL_TIMER_Object_t JoinLedTimer;
*/
/**
  * @brief Timer to trigger SCD41 pre-measurement (start stabilisation single shot, phase 1)
  */
static UTIL_TIMER_Object_t Scd41Timer;

/**
  * @brief Timer to discard the SCD41 stabilisation shot and start the useful shot (phase 2)
  */
static UTIL_TIMER_Object_t Scd41RestartTimer;

/**
  * @brief Timer to trigger SPS30 pre-measurement
  */
static UTIL_TIMER_Object_t Sps30Timer;

/**
  * @brief Timer to stop SPS30 fan cleaning
  */
static UTIL_TIMER_Object_t Sps30CleanupTimer;

/**
  * @brief Timer to end the BQ25185 CE pulse (re-enable charging)
  */
static UTIL_TIMER_Object_t ChargerCeTimer;

/**
  * @brief Periodic timer to reset the BQ25185 charger safety timer, independent of the TX loop
  */
static UTIL_TIMER_Object_t ChargerSafetyTimer;

/**
  * @brief Active TX duty cycle in seconds. Defaults to APP_TX_DUTYCYCLE, overridden by a
  *        flash-stored value at boot and by the downlink SET_TX_INTERVAL command at runtime.
  */
static uint32_t tx_dutycycle_s = APP_TX_DUTYCYCLE;

enum { EVENT_SCD_START=1U, EVENT_SCD_RESTART=2U, EVENT_SPS_START=4U, EVENT_SPS_STOP=8U };
static volatile uint32_t sensor_events;
static power_policy_t power_policy;
static sensor_t battery_sample;
static uint32_t battery_checked_at;
static bool modem_started, credentials_ready, sensors_started, recovery_stopped;
static uint32_t join_started_at, next_join_at, join_backoff_s = 300U;
static bool join_attempt_active;
static uint32_t tx_queued_at;
static bool tx_pending;
static uint16_t tx_failures, sensor_failures;
static uint16_t last_valid_sensors;
static uint8_t failed_uplinks;
static bool shutdown_pending;
static uint32_t next_measurement_at;

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

  // UTIL_TIMER_Create(&TxLedTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
  // UTIL_TIMER_Create(&RxLedTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
  // UTIL_TIMER_Create(&JoinLedTimer, LED_PERIOD_TIME, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);
  UTIL_TIMER_Create(&Scd41Timer, SCD41_PRE_MEASUREMENT_TIME_MS, UTIL_TIMER_ONESHOT, OnScd41TimerEvent, NULL);
  UTIL_TIMER_Create(&Scd41RestartTimer, SCD41_RESTART_TIME_MS, UTIL_TIMER_ONESHOT, OnScd41RestartTimerEvent, NULL);
  UTIL_TIMER_Create(&Sps30Timer, SPS30_PRE_MEASUREMENT_TIME_MS, UTIL_TIMER_ONESHOT, OnSps30TimerEvent, NULL);
  UTIL_TIMER_Create(&Sps30CleanupTimer, SPS30_CLEANING_DURATION_MS, UTIL_TIMER_ONESHOT, OnSps30CleanupTimerEvent, NULL);
  UTIL_TIMER_Create(&ChargerCeTimer, BQ25185_CE_RESET_PULSE_MS, UTIL_TIMER_ONESHOT, OnChargerCeTimerEvent, NULL);
  BQ25185_ChargeEnable();

  /* Independent periodic CE pulse to keep the BQ25185 safety timer from expiring */
  UTIL_TIMER_Create(&ChargerSafetyTimer, BQ25185_SAFETY_RESET_INTERVAL_MS, UTIL_TIMER_PERIODIC, OnChargerSafetyTimerEvent, NULL);
  UTIL_TIMER_Start(&ChargerSafetyTimer);

  /* USER CODE END LoRaWAN_Init_1 */

  if (!Nvm_Init())
  {
    Runtime_Fault(RUNTIME_FAULT_NVM);
  }

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
  UTIL_TIMER_Create(&SleepTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnSleepTimerEvent, NULL);
#endif /* (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0) */
  AppConfig_Load();
  (void)MAX17048_Init();
  (void)EnvSensors_Read(&battery_sample, SENSOR_FLAG_ONLY_BATTERY);
  PowerPolicy_Init(&power_policy, battery_sample.battery_voltage,
                   (battery_sample.valid & SENSOR_VALID_BATTERY) != 0U);
  battery_checked_at = SysTimeGetMcuTime().Seconds;
}

void LoRaWAN_Process(void)
{
  ServicePower();
  uint32_t sleep_time_ms = RUNTIME_MAX_SLEEP_MS;
  if (power_policy.mode != POWER_RECOVERY)
  {
    if (!modem_started)
    {
      smtc_modem_init(&Callbacks);
      smtc_modem_set_certification_mode(STACK_ID, CertMode);
      smtc_modem_set_crystal_error_ppm(BSP_CRYSTAL_ERROR);
      modem_started = true;
    }
    if (!sensors_started)
    {
      EnvSensors_Init();
      sensors_started = true;
      recovery_stopped = false;
    }
    sleep_time_ms = smtc_modem_run_engine();
    ServiceJoin();
    ProcessSensorEvents();
    /* Sensor work / joins can enqueue modem tasks: recompute before sleep. */
    sleep_time_ms = smtc_modem_run_engine();
    if (tx_pending && (uint32_t)(SysTimeGetMcuTime().Seconds - tx_queued_at) > 900U)
      Runtime_Fault(RUNTIME_FAULT_MODEM);
  }
  else
  {
    /* Expected dormancy is progress too; no radio or heavy sensor startup. */
    Runtime_ExpectProgress(POWER_CHECK_S + 120U);
  }
  if (sleep_time_ms > RUNTIME_MAX_SLEEP_MS) sleep_time_ms = RUNTIME_MAX_SLEEP_MS;
#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 0)
  /* Recheck pending work under the same interrupt mask used by STOP2. */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (sleep_time_ms > 0U && sensor_events == 0U &&
      (power_policy.mode == POWER_RECOVERY || !modem_started || !smtc_modem_is_irq_flag_pending()))
  {
    UTIL_TIMER_SetPeriod(&SleepTimer, sleep_time_ms);
    UTIL_TIMER_Start(&SleepTimer);
    UTIL_LPM_EnterLowPower();
  }
  __set_PRIMASK(primask);
#endif
}

static void ReadBattery(void)
{
  (void)EnvSensors_Read(&battery_sample, SENSOR_FLAG_ONLY_BATTERY);
  battery_checked_at = SysTimeGetMcuTime().Seconds;
  PowerPolicy_Update(&power_policy, battery_sample.battery_voltage,
                     (battery_sample.valid & SENSOR_VALID_BATTERY) != 0U,
                     battery_checked_at);
}

static void StopSensorTimers(void)
{
  UTIL_TIMER_Stop(&Scd41Timer);
  UTIL_TIMER_Stop(&Scd41RestartTimer);
  UTIL_TIMER_Stop(&Sps30Timer);
  UTIL_TIMER_Stop(&Sps30CleanupTimer);
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  sensor_events = 0U;
  __set_PRIMASK(primask);
}

static void ServicePower(void)
{
  bool checked = (uint32_t)(SysTimeGetMcuTime().Seconds - battery_checked_at) >= POWER_CHECK_S;
  if (checked) ReadBattery();
  if (power_policy.mode == POWER_RECOVERY)
  {
    if (!recovery_stopped)
    {
      StopSensorTimers();
      if (modem_started)
      {
        (void)smtc_modem_leave_network(STACK_ID);
        (void)smtc_modem_alarm_clear_timer();
      }
      tx_pending = false;
      join_attempt_active = false;
      next_join_at = SysTimeGetMcuTime().Seconds;
      tx_counter = 0U;
      shutdown_pending = true;
    }
    if (!recovery_stopped || (checked && shutdown_pending))
    {
      /* Also after a MCU-only reset: sensors may still be measuring. */
      shutdown_pending = EnvSensors_Sleep() != 0;
      if (shutdown_pending) (void)I2C2_RecoverBus();
    }
    sensors_started = false;
    recovery_stopped = true;
  }
  else recovery_stopped = false;
}

static void ServiceJoin(void)
{
  if (!credentials_ready || power_policy.mode == POWER_RECOVERY) return;
  uint32_t now = SysTimeGetMcuTime().Seconds;
  smtc_modem_status_mask_t status = 0;
  if (smtc_modem_get_status(STACK_ID, &status) != SMTC_MODEM_RC_OK) return;
  if (status & SMTC_MODEM_STATUS_JOINED)
  {
    join_attempt_active = false;
    join_backoff_s = 300U;
    return;
  }
  if (join_attempt_active)
  {
    if ((uint32_t)(now - join_started_at) < 300U) return;
    (void)smtc_modem_leave_network(STACK_ID);
    join_attempt_active = false;
    next_join_at = now + join_backoff_s;
    Runtime_ExpectProgress(join_backoff_s + 600U);
    if (join_backoff_s < 3600U) join_backoff_s *= 2U;
    if (join_backoff_s > 3600U) join_backoff_s = 3600U;
    return;
  }
  if ((int32_t)(now - next_join_at) < 0) return;
  if (smtc_modem_join_network(STACK_ID) == SMTC_MODEM_RC_OK)
  {
    join_attempt_active = true;
    join_started_at = now;
    Runtime_ExpectProgress(600U);
  }
  else next_join_at = now + 60U;
}

static void ProcessSensorEvents(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint32_t pending = sensor_events;
  sensor_events = 0U;
  __set_PRIMASK(primask);
  if (!pending) return;
  ReadBattery();
  ServicePower();
  if (power_policy.mode == POWER_RECOVERY) return;
  int32_t result = 0;
  if (pending & EVENT_SPS_STOP)
  {
    result |= SPS30_StopMeasurement();
    result |= SPS30_Sleep();
  }
  /* Do not start a stale premeasurement after its intended uplink. */
  if ((int32_t)(SysTimeGetMcuTime().Seconds - next_measurement_at) >= 0)
    pending &= EVENT_SPS_STOP;
  if (!(battery_sample.valid & SENSOR_VALID_BATTERY)) return;
  if (SCD41_ENABLED && (pending & EVENT_SCD_START))
    result |= EnvSensors_StartPreMeasurement(SENSOR_FLAG_CO2);
  if (SCD41_ENABLED && (pending & EVENT_SCD_RESTART))
    result |= EnvSensors_RestartPreMeasurement(SENSOR_FLAG_CO2);
  if (pending & EVENT_SPS_START)
    result |= EnvSensors_StartPreMeasurement(SENSOR_FLAG_SPS30);
  if (result != 0)
  {
    if (sensor_failures != UINT16_MAX) sensor_failures++;
    (void)I2C2_RecoverBus();
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

static bool ContextRange(modem_context_type_t type, uint32_t offset, uint32_t size, uint32_t *address)
{
  uint32_t base, capacity;
  switch (type)
  {
    case CONTEXT_LORAWAN_STACK: base=0; capacity=LORAWAN_CONTEXT_SIZE; break;
    case CONTEXT_MODEM: base=LORAWAN_CONTEXT_SIZE; capacity=MODEM_CONTEXT_SIZE; break;
    case CONTEXT_SECURE_ELEMENT: base=LORAWAN_CONTEXT_SIZE+MODEM_CONTEXT_SIZE; capacity=SECURE_ELEMENT_CONTEXT_SIZE; break;
    default: return false;
  }
  if (offset > capacity || size > capacity-offset) return false;
  *address=base+offset;
  return true;
}

static void RestoreContext(const modem_context_type_t type, uint32_t offset, uint8_t *buffer, const uint32_t size)
{
  uint32_t address;
  if (!ContextRange(type,offset,size,&address) || !Nvm_Read(address,buffer,size))
    Runtime_Fault(RUNTIME_FAULT_NVM);
}

static void StoreContext(const modem_context_type_t type, uint32_t offset, const uint8_t *buffer, const uint32_t size)
{
  uint32_t address;
  /* Never proceed with a Join after a nonce persistence failure. */
  if (!ContextRange(type,offset,size,&address) || !Nvm_Write(address,buffer,size))
    Runtime_Fault(RUNTIME_FAULT_NVM);
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
    smtc_modem_return_code_t event_status = smtc_modem_get_event(&current_event, &event_pending_count);
    if (event_status == SMTC_MODEM_RC_NO_EVENT) return;
    if (event_status != SMTC_MODEM_RC_OK) Runtime_Fault(RUNTIME_FAULT_MODEM);

    switch (current_event.event_type)
    {
      case SMTC_MODEM_EVENT_RESET:
      {
        APP_LOG(TS_OFF, VLEVEL_M, "Event received: RESET\r\n");

        uint8_t eui_bits = 0U;
        for (uint8_t i=0; i<sizeof user_dev_eui; ++i) eui_bits |= user_dev_eui[i];
        if (eui_bits == 0U) GetUniqueId(user_dev_eui);
        APP_LOG(TS_OFF, VLEVEL_M, "DevEUI: %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
                user_dev_eui[0],user_dev_eui[1],user_dev_eui[2],user_dev_eui[3],
                user_dev_eui[4],user_dev_eui[5],user_dev_eui[6],user_dev_eui[7]);

        /* Set user credentials */
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_deveui(stack_id, user_dev_eui));
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_joineui(stack_id, user_join_eui));
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_appkey(stack_id, user_gen_app_key));
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_nwkkey(stack_id, user_app_key));

        /* Set user region */
        ASSERT_SMTC_MODEM_RC(smtc_modem_set_region(stack_id, ACTIVE_REGION));

        /* Print Security material */
        /* Never print secret keys, including in debug builds. */
        CertMode = (smtc_modem_is_certification_port_disabled(STACK_ID)) ? 0 : CertMode;
        if (CertMode == false)
        {
          /* Schedule a Join LoRaWAN network */
          ASSERT_SMTC_MODEM_RC(smtc_modem_set_join_duty_cycle_backoff_bypass(stack_id, false));
          credentials_ready = true;
          next_join_at = SysTimeGetMcuTime().Seconds;
        }
        break;
      }
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
        // if (JoinLedTimer.IsRunning)
        // {
        //   UTIL_TIMER_Stop(&JoinLedTimer);
        //   HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */
        // }
        /* USER CODE END EventCallback_1 */
        if (CertMode == false)
        {
          /* Send first periodical uplink */
          SendTxData(LORAWAN_USER_APP_PORT);
        }
        break;

      case SMTC_MODEM_EVENT_TXDONE:
        tx_pending = false;
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: TXDONE\r\n");
        APP_LOG(TS_OFF, VLEVEL_H,  "Transmission done \r\n");
        smtc_modem_get_status(STACK_ID, &status_mask);
        /* USER CODE BEGIN EventCallback_2 */
        /* Check if the device has already joined a network */
        // if ((JoinLedTimer.IsRunning) && (status_mask & SMTC_MODEM_STATUS_JOINED) == SMTC_MODEM_STATUS_JOINED)
        // {
        //   UTIL_TIMER_Stop(&JoinLedTimer);
        //   HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */
        // }
        /* USER CODE END EventCallback_2 */
        break;

      case SMTC_MODEM_EVENT_DOWNDATA:
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: DOWNDATA\r\n");
        /* USER CODE BEGIN EventCallback_3 */
        //HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET); /* LED_BLUE */
        // UTIL_TIMER_Start(&RxLedTimer);
        /* USER CODE END EventCallback_3 */
        /* Get downlink data */
        if (smtc_modem_get_downlink_data(rx_payload, &rx_payload_size, &rx_metadata, &rx_remaining) != SMTC_MODEM_RC_OK)
          break;
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
        // if ((!JoinLedTimer.IsRunning) && (status_mask & SMTC_MODEM_STATUS_JOINED) != SMTC_MODEM_STATUS_JOINED)
        // {
        //   UTIL_TIMER_Start(&JoinLedTimer);
        // }
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
        (void)duty_cycle_status;
        APP_LOG(TS_OFF, VLEVEL_M,  "Event received: DUTY_CYCLE b"
                                   "usy %d\r\n", duty_cycle_status);
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

static void append_u16_be(uint8_t *buffer, uint8_t *index, uint16_t value)
{
  buffer[*index] = (uint8_t)((value >> 8) & 0xFFU);
  (*index)++;
  buffer[*index] = (uint8_t)(value & 0xFFU);
  (*index)++;
}

static void append_i16_be(uint8_t *buffer, uint8_t *index, int16_t value)
{
  append_u16_be(buffer, index, (uint16_t)value);
}

static void SendTxData(uint8_t port)
{
  /* USER CODE BEGIN SendTxData_1 */
  sensor_t sensor_data;
  uint8_t bufferSize = 0;
  uint8_t uplink_port = TX_PORT_ENV_BASE;

  (void)port;

  ReadBattery();
  ServicePower();
  if (power_policy.mode == POWER_RECOVERY) return;
  if (!(battery_sample.valid & SENSOR_VALID_BATTERY))
  {
    (void)smtc_modem_alarm_start_timer(POWER_CHECK_S);
    Runtime_ExpectProgress(POWER_CHECK_S + 120U);
    return;
  }
  /* Increment tx_counter */
  tx_counter++;
  if (tx_counter > 10U)
  {
    tx_counter = 1U;
  }

  /* Determine which sensors to read this cycle.
   * Base TX every 180 s. CO2 every 5th TX (15 min), SPS30 every 10th TX (30 min). */
  uint8_t sensor_flags = 0U;
  if (SCD41_ENABLED && tx_counter % 5U == 0U) { sensor_flags |= SENSOR_FLAG_CO2; }
  if (tx_counter % 10U == 0U) { sensor_flags |= SENSOR_FLAG_SPS30; }

  /* Read sensors */
  EnvSensors_Read(&sensor_data, sensor_flags);
  last_valid_sensors = sensor_data.valid;
  uint16_t expected = SENSOR_VALID_BATTERY | SENSOR_VALID_RHT | SENSOR_VALID_PRESSURE | SENSOR_VALID_UV;
  if (sensor_flags & SENSOR_FLAG_CO2) expected |= SENSOR_VALID_CO2;
  if (sensor_flags & SENSOR_FLAG_SPS30) expected |= SENSOR_VALID_PM;
  if ((sensor_data.valid & expected) != expected) if (sensor_failures != UINT16_MAX) sensor_failures++;

  /* Log raw stored values (scaling noted in unit label). Receiver / human
   * reader converts: T/100 = degC, RH/100 = %, P/10 = hPa. */
  APP_LOG(TS_ON, VLEVEL_M, "Sensors: T=%d [0.01degC], RH=%u [0.01%%], P=%u [0.1hPa], VBAT=%u mV\r\n",
          (int)sensor_data.temperature,
          (unsigned)sensor_data.humidity,
          (unsigned)sensor_data.pressure,
          (unsigned)sensor_data.battery_voltage);

  APP_LOG(TS_ON, VLEVEL_M, "LTR390: UVI=%u.%02u\r\n",
          (unsigned)(sensor_data.uvi_x100 / 100U),
          (unsigned)(sensor_data.uvi_x100 % 100U));

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

  if ((sensor_flags & SENSOR_FLAG_SPS30) != 0U)
  {
    uplink_port = TX_PORT_ENV_FULL;
  }
  else if ((sensor_flags & SENSOR_FLAG_CO2) != 0U)
  {
    uplink_port = TX_PORT_ENV_EXTENDED;
  }

  /* Wire format by port, all fields are 2-byte big-endian values:
   * - fPort 2: T, RH, P, VBAT, UV
   * - fPort 3: fPort2 + CO2
   * - fPort 4: fPort3 + PM mass + PM number + typ_size
   */
  append_i16_be(AppDataBuffer, &bufferSize, sensor_data.temperature);
  append_u16_be(AppDataBuffer, &bufferSize, sensor_data.humidity);
  append_u16_be(AppDataBuffer, &bufferSize, sensor_data.pressure);
  append_u16_be(AppDataBuffer, &bufferSize, sensor_data.battery_voltage);
  append_u16_be(AppDataBuffer, &bufferSize, sensor_data.uvi_x100);

  if ((sensor_flags & (SENSOR_FLAG_CO2 | SENSOR_FLAG_SPS30)) != 0U)
  {
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.co2_ppm);
  }

  if (sensor_flags & SENSOR_FLAG_SPS30)
  {
    uint32_t current_time_s = SysTimeGetMcuTime().Seconds;
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
      if (SPS30_StopMeasurement() != SPS30_STATUS_OK || SPS30_Sleep() != SPS30_STATUS_OK)
      {
        if (sensor_failures != UINT16_MAX) sensor_failures++;
        (void)I2C2_RecoverBus();
        (void)SPS30_Init();
      }
    }
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.pm1_0);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.pm2_5);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.pm4_0);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.pm10_0);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.nc_0_5);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.nc_1_0);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.nc_2_5);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.nc_4_0);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.nc_10_0);
    append_u16_be(AppDataBuffer, &bufferSize, sensor_data.typ_size);
  }

  APP_LOG(TS_ON, VLEVEL_M, "Uplink payload: fPort=%u, %u bytes\r\n", (unsigned)uplink_port, (unsigned)bufferSize);

  // if (JoinLedTimer.IsRunning)
  // {
  //   UTIL_TIMER_Stop(&JoinLedTimer);
  //   HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
  // }

  smtc_modem_return_code_t tx_status = smtc_modem_request_uplink(STACK_ID, uplink_port, false, AppDataBuffer, bufferSize);
  if (tx_status == SMTC_MODEM_RC_OK)
  {
    if (!tx_pending) tx_queued_at = SysTimeGetMcuTime().Seconds;
    tx_pending = true;
    failed_uplinks = 0U;
  }
  else
  {
    if (tx_failures != UINT16_MAX) tx_failures++;
    /* Duty-cycle and scheduling backpressure are expected, not a crash. */
    if (tx_status != SMTC_MODEM_RC_BUSY && tx_status != SMTC_MODEM_RC_NO_TIME &&
        ++failed_uplinks >= 5U) Runtime_Fault(RUNTIME_FAULT_MODEM);
  }

  if (EventType == TX_ON_TIMER)
  {
    smtc_modem_status_mask_t status_mask = 0;
    smtc_modem_get_status(STACK_ID, &status_mask);
    uint32_t dutycycle = (CertMode || ((status_mask & SMTC_MODEM_STATUS_JOINED) != SMTC_MODEM_STATUS_JOINED)) ? CERT_TX_DUTYCYCLE : tx_dutycycle_s;

    if (power_policy.mode == POWER_SAVE) dutycycle *= 2U;
    if (smtc_modem_alarm_start_timer(dutycycle) != SMTC_MODEM_RC_OK)
      Runtime_Fault(RUNTIME_FAULT_MODEM);
    Runtime_ExpectProgress(dutycycle + 600U);
    next_measurement_at = SysTimeGetMcuTime().Seconds + dutycycle;

    /* Schedule pre-measurement only for the next cycle where data is needed */
    uint8_t next_counter = (tx_counter % 10U) + 1U;

    if (SCD41_ENABLED && next_counter % 5U == 0U)
    {
      UTIL_TIMER_SetPeriod(&Scd41Timer, (dutycycle * 1000U > SCD41_PRE_MEASUREMENT_TIME_MS) ? dutycycle * 1000U - SCD41_PRE_MEASUREMENT_TIME_MS : 1U);
      UTIL_TIMER_Start(&Scd41Timer);
      UTIL_TIMER_SetPeriod(&Scd41RestartTimer, (dutycycle * 1000U > SCD41_RESTART_TIME_MS) ? dutycycle * 1000U - SCD41_RESTART_TIME_MS : 1U);
      UTIL_TIMER_Start(&Scd41RestartTimer);
    }

    if (next_counter % 10U == 0U)
    {
      UTIL_TIMER_SetPeriod(&Sps30Timer, (dutycycle * 1000U > SPS30_PRE_MEASUREMENT_TIME_MS) ? dutycycle * 1000U - SPS30_PRE_MEASUREMENT_TIME_MS : 1U);
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
      ReadBattery();
      if (power_policy.mode != POWER_NORMAL || !(battery_sample.valid & SENSOR_VALID_BATTERY)) break;
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
        last_sps30_clean_timestamp = SysTimeGetMcuTime().Seconds;
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
    case RX_CMD_SET_TX_INTERVAL:
    {
      if (size < 3U)
      {
        APP_LOG(TS_ON, VLEVEL_M, "SET_TX_INTERVAL: payload too short (size=%u)\r\n", (unsigned)size);
        break;
      }

      uint16_t new_interval = (uint16_t)(((uint16_t)payload[1] << 8) | (uint16_t)payload[2]);

      if ((new_interval < APP_TX_DUTYCYCLE_MIN_S) || (new_interval > APP_TX_DUTYCYCLE_MAX_S))
      {
        APP_LOG(TS_ON, VLEVEL_M, "SET_TX_INTERVAL: %u s out of range [%u..%u], ignored\r\n",
                (unsigned)new_interval, (unsigned)APP_TX_DUTYCYCLE_MIN_S, (unsigned)APP_TX_DUTYCYCLE_MAX_S);
        break;
      }

      if (AppConfig_SaveTxDutycycle(new_interval))
      {
        APP_LOG(TS_ON, VLEVEL_M, "SET_TX_INTERVAL: TX interval set to %u s (saved)\r\n", (unsigned)tx_dutycycle_s);
      }
      else
      {
        APP_LOG(TS_ON, VLEVEL_M, "SET_TX_INTERVAL: TX interval unchanged at %u s (flash save FAILED)\r\n", (unsigned)tx_dutycycle_s);
      }
      break;
    }
    case 0x12U:
    {
      uint8_t diagnostic[24] = {1U, (uint8_t)power_policy.mode};
      uint8_t n = 2U;
      uint32_t words[] = {SysTimeGetMcuTime().Seconds, Runtime_BootCount(), Runtime_ResetFlags()};
      for (uint8_t i = 0; i < 3U; ++i)
      { append_u16_be(diagnostic, &n, (uint16_t)(words[i] >> 16)); append_u16_be(diagnostic, &n, (uint16_t)words[i]); }
      append_u16_be(diagnostic, &n, (uint16_t)Runtime_LastFault());
      append_u16_be(diagnostic, &n, tx_failures);
      append_u16_be(diagnostic, &n, sensor_failures);
      append_u16_be(diagnostic, &n, last_valid_sensors);
      (void)smtc_modem_request_uplink(STACK_ID, 5U, false, diagnostic, n);
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
/*static bool IsAllZero(const uint8_t *buffer, uint8_t size)
{
  for (uint8_t i = 0; i < size; i++)
  {
    if (buffer[i] != 0U)
    {
      return false;
    }
  }

  return true;
}*/

//static void OnTxTimerLedEvent(void *context)
//{
  // HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET); /* LED_GREEN */
//}

//static void OnRxTimerLedEvent(void *context)
//{
  //HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); /* LED_BLUE */
//}

//static void OnJoinTimerLedEvent(void *context)
//{
  // HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin); /* LED_RED */
//}

static void OnScd41TimerEvent(void *context)
{
  (void)context;
  sensor_events |= EVENT_SCD_START;
}

static void OnScd41RestartTimerEvent(void *context)
{
  (void)context;
  sensor_events |= EVENT_SCD_RESTART;
}

static void OnSps30TimerEvent(void *context)
{
  (void)context;
  sensor_events |= EVENT_SPS_START;
}

static void OnSps30CleanupTimerEvent(void *context)
{
  (void)context;
  sensor_events |= EVENT_SPS_STOP;
}

static void ChargerSafetyTimerReset(void)
{
  BQ25185_ChargeDisable();
  UTIL_TIMER_Start(&ChargerCeTimer);
}

static void OnChargerCeTimerEvent(void *context)
{
  BQ25185_ChargeEnable();
}

static void OnChargerSafetyTimerEvent(void *context)
{
  ChargerSafetyTimerReset();
}

static void AppConfig_Load(void)
{
  app_config_t cfg = { 0 };

  if (!Nvm_Read(NVM_CONFIG_OFFSET, &cfg, sizeof(cfg)))
  {
    APP_LOG(TS_OFF, VLEVEL_M, "AppConfig: flash read failed, using default TX interval %u s\r\n",
            (unsigned)tx_dutycycle_s);
    return;
  }

  if ((cfg.magic == APP_CONFIG_MAGIC) &&
      (cfg.tx_dutycycle_s >= APP_TX_DUTYCYCLE_MIN_S) &&
      (cfg.tx_dutycycle_s <= APP_TX_DUTYCYCLE_MAX_S))
  {
    tx_dutycycle_s = cfg.tx_dutycycle_s;
    APP_LOG(TS_OFF, VLEVEL_M, "AppConfig: loaded TX interval %u s from flash\r\n", (unsigned)tx_dutycycle_s);
  }
  else
  {
    APP_LOG(TS_OFF, VLEVEL_M, "AppConfig: no valid config, using default TX interval %u s\r\n",
            (unsigned)tx_dutycycle_s);
  }
}

static bool AppConfig_SaveTxDutycycle(uint32_t dutycycle_s)
{
  app_config_t cfg;
  cfg.magic          = APP_CONFIG_MAGIC;
  cfg.tx_dutycycle_s = dutycycle_s;

  return Nvm_Write(NVM_CONFIG_OFFSET, &cfg, sizeof(cfg));
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
