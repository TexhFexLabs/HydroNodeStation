#include "standby.h"
#include "main.h"
#include "rtc.h"
#include "i2c.h"
#include "max17048.h"
#include "ina226.h"
#include "power_policy.h"
#include "runtime_health.h"
#include "stm32wlxx_LoRa_E5_mini_radio.h"

bool Standby_IwdgReady(void)
{
  return (FLASH->OPTR & FLASH_OPTR_IWDG_STDBY) == 0U;
}

/* The RTC runs in binary mode: SSR counts down at 1024 Hz. Alarm A on SSR,
 * like TIMER_IF_StartTimer(), but absolute and without the timer server. */
static void __attribute__((noreturn)) standby_sleep(void)
{
  RTC_AlarmTypeDef alarm = {0};

  (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  alarm.BinaryAutoClr = RTC_ALARMSUBSECONDBIN_AUTOCLR_NO;
  alarm.AlarmTime.SubSeconds = RTC->SSR - ((STANDBY_WAKE_MIN * 60U) << RTC_N_PREDIV_S);
  alarm.AlarmMask = RTC_ALARMMASK_NONE;
  alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDBINMASK_NONE;
  alarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BCD) != HAL_OK)
  {
    /* Without a wake-up Standby would never end: reset and retry instead. */
    NVIC_SystemReset();
  }
  /* RTC alarm wakes the CPU from Standby through the internal line. */
  HAL_PWREx_EnableInternalWakeUpLine();
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
  HAL_PWR_EnterSTANDBYMode();
  NVIC_SystemReset();
  for (;;) { }
}

void Standby_CheckWake(void)
{
  MAX17048_Data_t battery;

  if (!__HAL_PWR_GET_FLAG(PWR_FLAG_SB)) return;
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
  /* Only I2C and the gauge; it kept measuring in hibernate. A reading that
   * fails is no undervoltage: boot normally and let RECOVERY decide. */
  MX_I2C2_Init();
  if (MAX17048_Init() != MAX17048_OK || MAX17048_Read(&battery) != MAX17048_OK ||
      battery.voltage_mv >= POWER_RESTART_MV)
  {
    return;
  }
  HAL_PWR_EnableBkUpAccess();
  MX_RTC_Init();
  standby_sleep();
}

void Standby_Enter(void)
{
  (void)INA226_Shutdown();
  BSP_RADIO_SwitchPowerOff();
  Runtime_FaultDetail(0U);
  /* Reason 8 in DR6: the boot after the final wake-up reports 0x0540 with it.
   * The quick checks in between leave it untouched. */
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR6, RUNTIME_REASON_STANDBY);
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR7, 0U);
  Runtime_BlinkCode(RUNTIME_REASON_STANDBY);
  __disable_irq();
  standby_sleep();
}
