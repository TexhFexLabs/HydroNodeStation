/**
 ******************************************************************************
 * @file    debug_profile.c
 * @brief   Debug profile: activated when PB4 (DEBUG_SW_Pin) is HIGH at boot.
 *
 * Reads ALL sensors every cycle and dumps values via APP_LOG.
 * Never returns — the normal LoRaWAN program is not started in this mode.
 *
 * Sensor timing per cycle (~8 s):
 *   t =   0 ms  Start CO2 single-shot + SPS30 continuous measurement.
 *   t ~   0 ms  Read basic sensors (T, RH, P, VBAT, UV) — no pre-warm needed.
 *   t += 5500 ms Wait for SCD41 single-shot to complete (~5 s per datasheet).
 *   t ~5500 ms  Read CO2 + SPS30 (both ready by now).
 *   t += 2000 ms Hold before next cycle.
 ******************************************************************************
 */

#include "debug_profile.h"
#include "sys_app.h"       /* APP_LOG, VLEVEL_M, TS_ON, TS_OFF */
#include "sys_sensors.h"   /* sensor_t, EnvSensors_Read, EnvSensors_StartPreMeasurement, SENSOR_FLAG_* */
#include "stm32wlxx_hal.h" /* HAL_Delay */

/* Time to wait after starting the SCD41 single-shot before reading (ms).
 * SCD41 datasheet: single-shot takes ~5 s; 5500 ms gives safe margin. */
#define DBG_CO2_WAIT_MS     5500U

/* Pause between CO2/SPS30 read and next cycle start (ms). */
#define DBG_CYCLE_PAUSE_MS  2000U

void DebugProfile_Run(void)
{
    APP_LOG(TS_OFF, VLEVEL_M, "\r\n=== DEBUG PROFILE (PB4 HIGH) ===\r\n");
    APP_LOG(TS_OFF, VLEVEL_M, "All sensors, every ~8 s. Reset to exit.\r\n\r\n");

    for (;;)
    {
        sensor_t d = {0};

        /* Start slow sensors now so wait is shared */
        (void)EnvSensors_StartPreMeasurement(SENSOR_FLAG_CO2);
        (void)EnvSensors_StartPreMeasurement(SENSOR_FLAG_SPS30);

        /* Read fast sensors immediately (T, RH, P, VBAT, UV) */
        (void)EnvSensors_Read(&d, 0U);

        int      t_int  = (int)(d.temperature / 100);
        unsigned t_frac = (unsigned)(d.temperature < 0
                                     ? -(d.temperature % 100)
                                     :  (d.temperature % 100));

        APP_LOG(TS_ON, VLEVEL_M,
                "T=%d.%02u C  RH=%u.%02u %%  P=%u.%01u hPa  VBAT=%u mV\r\n",
                t_int, t_frac,
                (unsigned)(d.humidity  / 100U), (unsigned)(d.humidity  % 100U),
                (unsigned)(d.pressure  /  10U), (unsigned)(d.pressure  %  10U),
                (unsigned) d.battery_voltage);

        APP_LOG(TS_ON, VLEVEL_M, "UVI=%u.%02u\r\n",
                (unsigned)(d.uvi_x100 / 100U),
                (unsigned)(d.uvi_x100 % 100U));

        /* Wait for SCD41 single-shot, then read CO2 + SPS30 */
        HAL_Delay(DBG_CO2_WAIT_MS);
        (void)EnvSensors_Read(&d, SENSOR_FLAG_CO2 | SENSOR_FLAG_SPS30);

        APP_LOG(TS_ON, VLEVEL_M, "CO2=%u ppm\r\n", (unsigned)d.co2_ppm);

        APP_LOG(TS_ON, VLEVEL_M,
                "PM MC [0.1 ug/m3]: 1.0=%u  2.5=%u  4.0=%u  10=%u\r\n",
                (unsigned)d.pm1_0, (unsigned)d.pm2_5,
                (unsigned)d.pm4_0, (unsigned)d.pm10_0);

        APP_LOG(TS_ON, VLEVEL_M,
                "PM NC [0.1 #/cm3]: 0.5=%u  1.0=%u  2.5=%u  4.0=%u  10=%u  TypSize=%u nm\r\n",
                (unsigned)d.nc_0_5,  (unsigned)d.nc_1_0,  (unsigned)d.nc_2_5,
                (unsigned)d.nc_4_0,  (unsigned)d.nc_10_0, (unsigned)d.typ_size);

        APP_LOG(TS_OFF, VLEVEL_M, "---\r\n");

        HAL_Delay(DBG_CYCLE_PAUSE_MS);
    }
}
