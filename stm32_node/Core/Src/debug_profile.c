/**
 ******************************************************************************
 * @file    debug_profile.c
 * @brief   Debug profile: active when DIP 3 (PB4 to GND) is closed at boot.
 *
 * Reads ALL sensors every cycle and prints them on USART1 (H1, 115200 8N1)
 * with its own small output path, so it also works in Release builds where
 * APP_LOG is compiled out. Never returns, LoRaWAN is not started.
 *
 * Sensor timing per cycle (~19 s):
 *   t =      0  Start CO2 single shot and SPS30; read the fast sensors,
 *               INA226, solar ADC, gauge, board temperature, counters.
 *   t =  5.5 s  Discard the SCD41 stabilisation shot, start the useful one.
 *   t = 16.5 s  Read CO2 and SPS30, pause 2 s.
 ******************************************************************************
 */

#include <stdarg.h>
#include "debug_profile.h"
#include "runtime_health.h"
#include "sys_sensors.h"   /* sensor_t, EnvSensors_Read, EnvSensors_StartPreMeasurement, SENSOR_FLAG_* */
#include "stm32wlxx_hal.h" /* HAL_Delay */
#include "stm32_tiny_vsnprintf.h"
#include "usart_if.h"
#include "ina226.h"
#include "max17048.h"
#include "adc_if.h"
#include "pulse_counter.h"

/* Time to wait after starting the SCD41 single-shot before reading (ms).
 * SCD41 datasheet: single-shot takes ~5 s; 5500 ms gives safe margin. */
#define DBG_CO2_WAIT_MS     5500U

/* Pause between CO2/SPS30 read and next cycle start (ms). */
#define DBG_CYCLE_PAUSE_MS  2000U

static void dbg_printf(const char *fmt, ...)
{
    char line[128];
    va_list args;
    va_start(args, fmt);
    (void)tiny_vsnprintf_like(line, (int)sizeof line, fmt, args);
    va_end(args);
    DebugUart_Write(line);
}

/* Signed 0.01 units as "-1.23"; the sentinel prints as "--". */
static void dbg_x100(const char *name, int32_t v, const char *unit)
{
    if (v == INT16_MIN) { dbg_printf("%s=-- %s  ", name, unit); return; }
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    dbg_printf("%s=%s%u.%02u %s  ", name, v < 0 ? "-" : "", (unsigned)(a / 100U), (unsigned)(a % 100U), unit);
}

void DebugProfile_Run(void)
{
    DebugUart_Start();
    dbg_printf("\r\n=== DEBUG PROFILE (DIP 3) ===\r\nAll sensors, every ~19 s. Reset to exit.\r\n\r\n");

    Runtime_ExpectProgress(0U);
    EnvSensors_Init();
    Runtime_Process();
    for (;;)
    {
        sensor_t d = {0};
        INA226_Data_t ina;
        MAX17048_Gauge_t gauge;
        uint16_t solar_adc_mv, c1, c2;
        int16_t board_t;

        /* Start slow sensors now so wait is shared */
        (void)EnvSensors_StartPreMeasurement(SENSOR_FLAG_CO2);
        (void)EnvSensors_StartPreMeasurement(SENSOR_FLAG_SPS30);

        /* Read fast sensors immediately (T, RH, P, VBAT, UV) */
        (void)EnvSensors_Read(&d, 0U);
        dbg_x100("T", d.temperature, "C");
        dbg_x100("RH", d.humidity == 0xFFFFU ? INT16_MIN : (int32_t)d.humidity, "%");
        dbg_printf("P=%u.%u hPa  VBAT=%u mV  ", (unsigned)(d.pressure / 10U), (unsigned)(d.pressure % 10U),
                   (unsigned)d.battery_voltage);
        dbg_x100("UVI", d.uvi_x100 == 0xFFFFU ? INT16_MIN : (int32_t)d.uvi_x100, "");
        dbg_printf("\r\n");

        if (INA226_Measure(&ina) == INA226_OK || (INA226_Init() == INA226_OK && INA226_Measure(&ina) == INA226_OK))
        {
            uint32_t p_mw = ((uint32_t)ina.bus_mv * ina.current_01ma + 5000U) / 10000U;
            dbg_printf("Solar INA226: %u mV  %u.%u mA  %u mW  ", (unsigned)ina.bus_mv,
                       (unsigned)(ina.current_01ma / 10U), (unsigned)(ina.current_01ma % 10U), (unsigned)p_mw);
        }
        else dbg_printf("Solar INA226: --  ");
        if (SYS_ReadSolarMv(&solar_adc_mv)) dbg_printf("ADC PB2: %u mV\r\n", (unsigned)solar_adc_mv);
        else dbg_printf("ADC PB2: --\r\n");

        (void)MAX17048_ReadGauge(&gauge);
        dbg_x100("SOC", gauge.soc_x100 == 0xFFFFU ? INT16_MIN : (int32_t)gauge.soc_x100, "%");
        dbg_x100("Rate", gauge.crate_x100, "%/h");
        if (!SYS_ReadBoardTemperature(&board_t)) board_t = INT16_MIN;
        dbg_x100("Board", board_t, "C");
        PulseCounter_Read(&c1, &c2);
        if (c1 != PULSE_MISSING) dbg_printf("CNT1=%u  CNT2=%u", (unsigned)c1, (unsigned)c2);
        dbg_printf("\r\n");

        /* Wait for SCD41 single-shot, then read CO2 + SPS30 */
        HAL_Delay(DBG_CO2_WAIT_MS);
        Runtime_Process();
        (void)EnvSensors_RestartPreMeasurement(SENSOR_FLAG_CO2);
        HAL_Delay(DBG_CO2_WAIT_MS);
        Runtime_Process();
        HAL_Delay(DBG_CO2_WAIT_MS); /* 16.5 s total SPS30 settling */
        Runtime_Process();
        (void)EnvSensors_Read(&d, SENSOR_FLAG_CO2 | SENSOR_FLAG_SPS30);

        dbg_printf("CO2=%u ppm\r\n", (unsigned)d.co2_ppm);
        dbg_printf("PM MC [0.1 ug/m3]: 1.0=%u  2.5=%u  4.0=%u  10=%u\r\n",
                   (unsigned)d.pm1_0, (unsigned)d.pm2_5, (unsigned)d.pm4_0, (unsigned)d.pm10_0);
        dbg_printf("PM NC [0.1 #/cm3]: 0.5=%u  1.0=%u  2.5=%u  4.0=%u  10=%u  TypSize=%u nm\r\n",
                   (unsigned)d.nc_0_5, (unsigned)d.nc_1_0, (unsigned)d.nc_2_5,
                   (unsigned)d.nc_4_0, (unsigned)d.nc_10_0, (unsigned)d.typ_size);
        dbg_printf("---\r\n");

        HAL_Delay(DBG_CYCLE_PAUSE_MS);
        Runtime_Process();
    }
}
