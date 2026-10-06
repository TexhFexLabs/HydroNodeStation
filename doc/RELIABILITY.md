# Long-term operation (firmware 1.6 and 2.0)

Firmware 2.0 runs on PCB 1.1 and keeps every mechanism of 1.6 below. What 2.0 adds for the new board is in [Firmware 2.0 (PCB 1.1)](#firmware-20-pcb-11) at the end.

The reproducible outage is in the STM32 LoRaWAN port: `SysTimeToMs(SysTimeGet()) / 1000` produces seconds from a wrapping 32-bit millisecond value. After 49.710 days, an alarm spanning the wrap has a deadline larger than the largest value this clock can ever return. No further periodic uplink occurs, even with a full battery. This defect matches the reported symptom; the historical field failures were not captured with a debugger.

## Implemented changes

| Area | Behavior in 1.6 |
|---|---|
| Time | All 44 affected middleware expressions use monotonic MCU seconds. The RTC epoch read accounts for a pending underflow interrupt and a wrap during the read. HAL_GetTick returns milliseconds; a temporary SysTick provides bounded startup timeouts. |
| Recovery | Independent IWDG starts before peripheral initialization. STOP2 sleeps are capped at 8 seconds; only thread-mode progress feeds the watchdog. Missing measurement cycles have a separate deadline (interval + 600 seconds); queued TX has a 900-second limit. Faults reset the MCU and preserve a reason in RTC backup registers. |
| Sensor scheduling | Sensor timer callbacks set flags only. The main loop serializes I2C work; the SCD41 driver no longer deinitializes the bus underneath other sensors. Late premeasurement events are discarded. Short device command delays remain bounded main-loop waits. |
| SPS30 | Stop waits 20 ms before Sleep; command errors propagate; initialization handles a sensor still measuring after a MCU-only reset. Failed shutdown attempts trigger recovery. A true sensor power cycle still requires suitable hardware. |
| Battery | Checks precede heavy work. NORMAL, SAVE and RECOVERY have hysteresis. Missing battery measurements never enable sensor loads or flash writes. Radio/sensor initialization is deferred at a low-voltage boot. |
| Joining | Standard modem join backoff is enabled. Unsuccessful attempts run in at most five-minute windows, followed by 5/10/20/40/60-minute pauses. There is no periodic reboot merely because the gateway is absent. |
| Flash | Two CRC-protected snapshots with sequence and final commit marker. Only the inactive page is erased. Legacy context/config is migrated without erasing the old pages. Unchanged data is not rewritten; failed nonce writes stop the join. |
| Data | Missing sensor values use explicit sentinels. The supplied decoder handles negative temperatures, missing data and malformed frames. Port 4 retains its CO2 slot even when SCD41 is disabled. |
| Credentials | `se-identity-local.h` supplies ignored local credentials. Tracked header contains defaults and no hidden skip-worktree changes. Debug logs print DevEUI, never keys. |
| Energy | Normal cadence is unchanged. Production UART/DWT reinitialization and activity LED are disabled. SPS30 sleep sequencing is corrected. No measured percentage saving is claimed. |

## Battery policy

Defaults are conservative starting values for a 1S LiPo, configurable in `Core/Inc/power_policy.h`. Validate against the actual cell and load behavior.

- Normal: default 180-second uplinks, CO2 every fifth and PM every tenth uplink.
- Below 3500 mV: SAVE doubles the interval. Return to NORMAL at 3650 mV.
- Below 3300 mV, or after three failed voltage reads: RECOVERY cancels sensor timers and radio activity. Check the battery every 60 seconds; wake briefly every at most 8 seconds for watchdog supervision.
- Recovery exit: at least 3600 mV on checks spanning 60 seconds. Below this threshold or on a failed reading, restart the stability interval. Sensor setup and join then resume automatically.
- A boot with insufficient/unknown voltage starts in RECOVERY. A healthy boot does not need the recovery delay.

The firmware cannot physically disconnect the permanently supplied sensors or protect an unprotected cell below MCU operating voltage. In the supplied schematic, BQ25185 TS/MR uses a fixed 10-kOhm resistor, so it does not measure battery temperature. An appropriate battery NTC and verified hardware undervoltage/restart path remain necessary for outdoor winter operation. The charger CE pulse remains independent every two hours; changing a working charging circuit without measured status/temperature feedback would be unjustified. The physical regulator workaround was not identified as the reported outage cause.

## Persistence and flashing

Application code is limited to 248 KiB. `0x0803E000` and `0x0803E800` store new snapshots. `0x0803F000` and `0x0803F800` remain the legacy context/config; an unused doubleword at `0x0803F7F8` marks migration. Do not mass-erase these pages during a routine upgrade. Do not downgrade to legacy firmware using stale nonce records; reprovision if a downgrade is required. Boot after an interrupted first migration uses a valid new snapshot or the original context. Once migration is marked, two invalid snapshots cause a fault instead of silently restoring obsolete nonces.

The first page rewrite implementation has been bypassed for application and modem state. `flash_if.c` remains available for legacy interfaces but is no longer used by these persistence callbacks. Read/write callbacks validate context type, offset and size. Flash writes require a valid battery reading at or above the recovery threshold. Reads of interrupted flash records handle ECC errors only while accessing those records; other CPU/NMI faults reset.

A write is reported successful once its snapshot is committed and verified. The migration marker is set afterwards and never decides the result: reporting a committed write as lost would reset the MCU over data that is safe. Because the legacy `flash_if.c` reprogrammed whole pages including their unused tail, the marker doubleword reads erased on an upgraded board but cannot be programmed again; the marker step therefore erases the legacy context page once before retrying. That page is obsolete at this point, since the snapshot being marked already carries the migrated content.

A fault blinks its reason on the diagnostic LED (PB3), then after a pause the failing step, and stores both in `RTC_BKP_DR6`/`DR7` for the next boot and the `0x12` diagnostic. Without this an unattended reset is undiagnosable: trace and debugger are disabled in the production build.

The modem performs OTAA after a restart; this is not full session continuation. Repeated power loss, flash endurance, oscillator failures and supply ramps still require physical fault-injection tests.

## Payload compatibility and diagnostics

Firmware 2.0 sends 32/34/32 bytes on fPort 2/3/4 (ports 2 and 3 end in the solar and fuel gauge block) and fault frames on fPort 99, see [CONFIGURATION.md](CONFIGURATION.md). Scaling of the base fields is unchanged. Missing unsigned fields are `0xFFFF`; missing signed fields are `0x8000`. Consumers must decode these as missing, not large numeric readings. Use [payload-decoder.js](payload-decoder.js); it still reads 1.x frames. The HydroNode backend marks such channels with a per-channel missing value and reads fPort 99 without a layout.

Downlink `0x12` on fPort 2 requests a diagnostic uplink on fPort 5 (26 bytes, big-endian; byte 14 is the fault reason, byte 15 the failing step, bytes 22-23 the modem anomaly count, byte 24 the NVM error count and byte 25 the last NVM error). This optional response can be deferred/rejected by a busy modem; it is not sent automatically every cycle.

| Offset | Field |
|---|---|
| 0 | Version = 1 (u8) |
| 1 | Power mode: 0 normal, 1 save, 2 recovery (u8) |
| 2 | MCU RTC seconds (u32; RTC can retain time across software resets) |
| 6 | Boot count (u32, RTC backup retained while powered) |
| 10 | RCC reset flags captured at boot (u32) |
| 14 | Previous explicit fault: 0 none, 1 HAL, 2 CPU, 3 modem, 4 missing progress, 5 NVM, 6 LSE, 7 command FF, 8 Standby wake-up (u16) |
| 16 | Uplink-request error count this boot (u16, saturating) |
| 18 | Sensor error count this boot (u16, saturating) |
| 20 | Last measurement validity bits: battery, T/RH, pressure, UV, CO2, PM (u16) |

## Validation

Run from the repository root:

```
python3 stm32_node/tests/run_tests.py
node stm32_node/tests/test_decoder.js
cmake -S stm32_node -B stm32_node/build/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build stm32_node/build/Release
```

Host tests compile the actual health module, power policy, NVM store, SPS30/SCD41 drivers, extracted modem alarm setter/supervisor function and extracted RTC epoch reader. They cover five simulated years / 876,000 alarms, 208 interrupted-write scenarios, CRC fallback, migration, invalid battery reads, hysteresis, SPS30 command timing and RTC interrupt ordering. These tests do not emulate an entire STM32 or RF network.

Resets are confined to conditions nothing else can recover from: CPU faults, the HAL error handler, a broken modem event queue, a transmission stuck for 15 minutes, five consecutive non-busy uplink rejections, a lost alarm timer, and the progress deadline. Modem panics, context read/write failures and an unreadable store are counted and reported, not reset: their conditions either resolve on their own or persist in flash, where a reset only loops. The stack's own panic sites are reachable in normal operation - the no-downlink threshold trips after 2400 uplinks, roughly five days at the default duty cycle.

Before field deployment, verify watchdog recovery, STOP2 current, RTC rollover, missing/shorted sensors, no-gateway behavior, voltage ramps with load spikes, actual flash power cuts and decoder compatibility on the board. Run a soak test beyond the former 50-day failure point. Firmware was built and host-tested, not flashed remotely.

## Energy claims

The study's roughly 1.1-mA baseline alone represents 26.4 mAh/day. Finding its physical cause can save more than small arithmetic optimizations. Its 77-mC CO2 entry is for one shot; this firmware intentionally takes two shots per useful reading. Its SPS30 event duration also differs from the current premeasurement schedule. Reintegrate current traces for the same firmware, battery-side voltage, radio conditions and measurement cadence before claiming a percentage saving. A doubled interval means 50% fewer periodic uplinks, not 50% less total power.

Hardware sources: [BQ25185 datasheet](https://www.ti.com/lit/ds/symlink/bq25185.pdf), repository `hardware/datasheets/sps30_datasheet.pdf` (Table 8, Sleep only in Idle), `hardware/datasheets/scd4x_low_power_appnote.pdf` (discard first single shot after power-down). Original study and schematic exports are retained as historical project documents.

Local verification on 2026-09-06: Release 88,144 B ROM, Debug 168,936 B ROM, CO2-disabled Release 87,000 B ROM. All three link successfully within the reserved flash limit. Remaining three compiler warnings are existing middleware variables unused when tracing is disabled.

## Firmware 2.0 (PCB 1.1)

| Area | Behavior in 2.0 |
|---|---|
| LSE | Started by the application, not by `HAL_RCC_OscConfig`: drive MEDIUMHIGH, 2 s for LSERDY, then LSE off, drive HIGH and a second attempt. A running LSE after a system reset is left alone. If the second attempt fails too, reason 6 goes to DR6, the code blinks on LED_DIAG and the MCU restarts after 60 s; a later cold start can succeed. There is no LSI fallback: the RTC times the LoRaWAN receive windows. A start that needed the second attempt is reported as `0x0542`. |
| Radio clock and PA | 32 MHz crystal X1, no TCXO (PB0 is never driven). Only RFO_HP is wired; `paSelect` follows the TX configuration instead of a fixed RFO_LP. The stack asks for EIRP; the PA gets that minus `BOARD_ANTENNA_GAIN_DB` (default 2 dB), so a 2 dBi antenna stays at 16 dBm EIRP. Change the define with the antenna. |
| RF switch | PB12 supplies the BGS12SN6 only while the radio works. The modem's TCXO hooks drive it: before every TX or RX task CTRL low, PB12 high, 1 ms settle; after every radio sleep CTRL low, PB12 low. The modem gets a 2 ms startup delay, so TX and both receive windows start early by that much and RX1/RX2 do not move. Receiving uses CTRL low (RF1), sending CTRL high (RF2); CTRL only changes while the switch is supplied. The radio planner sleeps between TX and RX1; the next task switches the supply on again. |
| ADR and link check | After every join the ADR profile is network controlled. 24 h after the join and then daily a LinkCheckReq leaves as its own MAC uplink after a regular TX done. Three unanswered days in a row: the station leaves the network, stops its timers and joins again with the normal backoff; the rejoin is reported as `0x0430` with the days. Every rejoin uses a DevNonce. Link checks in installation mode do not count as silent days. |
| Supply rails | 5 V is on from boot (after 5 ms for the regulator and 100 ms for the SPS30), also when the boot starts in RECOVERY. 3V3SWITCHABLE stays off. Levels are set before the pin mode changes. |
| Standby | A second stage below RECOVERY for deep discharge, see [CONFIGURATION.md](CONFIGURATION.md#power-modes). It needs the option byte IWDG_STDBY cleared; without it the station stays in RECOVERY and reports `0x0544`. The hourly checks in Standby do not count as boots; the boot that joins again reports `0x0540` with reason 8. |
| Fault reports | Missing parts, read errors, supply states, rejoins, rejected uplinks, NVM errors and every restart reach the backend on fPort 99 instead of only blinking on a board nobody watches. Urgent entries leave with the next uplink, the rest at most hourly; flapping parts are damped for 6 h and radio counters come once a day, so a bad sensor or poor coverage does not eat the airtime. |
| Diagnostics | Trace and the debug profile write over USART1 on H1 (PB6/PB7) only while they print. Probe lines are gone; a radio monitor output on a board pin breaks the build. |

Hardware verification of 2.0 (RF timing, levels, currents, LSE cold start, Standby wake-up) is tracked in the field acceptance test and still open.

