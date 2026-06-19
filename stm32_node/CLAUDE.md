# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Run all commands from `stm32_node/`.

```bash
cmake -B build/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Release
```

ELF/binary output lands in `build/Release/`.

## Architecture

### Boot flow
`main.c` → `MX_GPIO_Init()` + `MX_I2C2_Init()` + `MX_LoRaWAN_Init()` → debug-pin check (`DEBUG_SW_Pin` HIGH → `DebugProfile_Run()`, never returns) → 10 s LED blink → `MX_LoRaWAN_Process()` superloop.

`MX_LoRaWAN_Init()` calls `LoRaWAN_Init()` in `lora_app.c`, which initialises the Semtech LBM modem (`smtc_modem_init`), then calls `EnvSensors_Init()`.

`MX_LoRaWAN_Process()` calls `LoRaWAN_Process()` → `smtc_modem_run_engine()` → enters STOP2 via `UTIL_LPM_EnterLowPower()` for the returned sleep duration.

### Key source files
| File | Role |
|------|------|
| `LoRaWAN/App/lora_app.c` | Main app logic: sensor scheduling, TX payload encoding, downlink command handling |
| `LoRaWAN/App/lora_app.h` | Key constants: `APP_TX_DUTYCYCLE` (300 s), battery thresholds, pre-measurement timings |
| `Core/Src/sys_sensors.c` | Sensor abstraction: `EnvSensors_Read()`, `EnvSensors_Init()`, `EnvSensors_StartPreMeasurement()` |
| `Core/Inc/sys_sensors.h` | `sensor_t` struct with all fields and `SENSOR_FLAG_*` bitmasks |
| `Core/Inc/sys_conf.h` | Build-time toggles: `LOW_POWER_DISABLE`, `APP_LOG_ENABLED`, `SCD41_ENABLED` |
| `LoRaWAN/App/se-identity.h` | LoRaWAN keys (DEV_EUI, JOIN_EUI, APP_KEY) |

Individual sensor drivers live in `Core/Src/`: `sht45.c`, `bmp390.c`, `ltr390.c`, `max17048.c`, `scd41.c`, `sps30.c`.

### Hardware target
Release builds for the **STM32WLE5CCU6** custom PCB (UFQFPN48): define `STM32WLE5xx`, 1-pin BGS12SN6 RF switch at PC13, startup file `startup_stm32wle5xx.s`, `-Os`. These are set in `cmake/stm32cubemx/CMakeLists.txt`.

A Debug target (STM32WL55xx, Wio-E5 mini dev board) exists as legacy — do not remove it, but all active development targets Release only.

### Sensor scheduling & TX encoding
`tx_counter` cycles 1–6 each uplink:
- Every TX: SHT45 (temp/RH), BMP390 (pressure), LTR390 (UV), MAX17048 (battery)
- Every 3rd TX (counter % 3 == 0): + SCD41 CO2 → uplink on **fPort 3**
- Every 6th TX (counter % 6 == 0): + SPS30 particulates → uplink on **fPort 4**
- Otherwise: **fPort 2**

Slow sensors (SCD41: 5.5 s, SPS30: 16.5 s) need a pre-measurement wake-up before the TX window; timers `Scd41Timer` / `Sps30Timer` in `lora_app.c` handle this.

Wire format: all fields are **2-byte big-endian**. Scaling: temperature ÷ 100 = °C, humidity ÷ 100 = %RH, pressure ÷ 10 = hPa, uvi_x100 ÷ 100 = UVI, PM mass ÷ 10 = µg/m³, NC ÷ 10 = #/cm³.

Low-battery logic doubles the duty cycle below 3400 mV and multiplies by 12× below 3000 mV (only base sensors sent).

### Trace output
HW USART1 is unused. All `APP_LOG` output goes through **SW-UART on PA6** (bit-banged, `Core/Src/sw_uart.c`). To see logs, attach a USB-UART on PA6 at the baud rate configured in `sw_uart.h`. Enable logs by setting `APP_LOG_ENABLED 1` in `sys_conf.h`.

### Debug profile mode
Pull `DEBUG_SW_Pin` (PB4) HIGH before boot → `DebugProfile_Run()` reads all sensors in a tight loop and logs via SW-UART. Skips LoRaWAN entirely. Useful for sensor bring-up without waiting for TX windows.

### STM32CubeMX generated code
Files use `/* USER CODE BEGIN */` / `/* USER CODE END */` guards. Only edit inside those sections for peripheral init files (`gpio.c`, `i2c.c`, `usart.c`, etc.) — regenerating via CubeMX will otherwise overwrite changes. `lora_app.c` and `sys_sensors.c` are custom app code written entirely inside USER CODE sections.
