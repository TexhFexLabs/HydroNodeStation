# Long-term operation (firmware 1.6)

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

The modem performs OTAA after a restart; this is not full session continuation. Repeated power loss, flash endurance, oscillator failures and supply ramps still require physical fault-injection tests.

## Payload compatibility and diagnostics

fPort 2/3/4 keep 10/12/32 bytes and unchanged scaling for valid measurements. Missing unsigned fields are `0xFFFF`; missing temperature is `0x8000`. Consumers must decode these as missing, not large numeric readings. Use [payload-decoder.js](payload-decoder.js). The HydroNode backend is a separate project and was not changed here; its decoder needs equivalent sentinel handling before rollout.

Downlink `0x12` on fPort 2 requests a diagnostic uplink on fPort 5 (22 bytes, big-endian). This optional response can be deferred/rejected by a busy modem; it is not sent automatically every cycle.

| Offset | Field |
|---|---|
| 0 | Version = 1 (u8) |
| 1 | Power mode: 0 normal, 1 save, 2 recovery (u8) |
| 2 | MCU RTC seconds (u32; RTC can retain time across software resets) |
| 6 | Boot count (u32, RTC backup retained while powered) |
| 10 | RCC reset flags captured at boot (u32) |
| 14 | Previous explicit fault: 0 none, 1 HAL, 2 CPU, 3 modem, 4 missing progress, 5 NVM (u16) |
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

Host tests compile the actual health module, power policy, NVM store, SPS30 driver, extracted modem alarm setter/supervisor function and extracted RTC epoch reader. They cover five simulated years / 876,000 alarms, 208 interrupted-write scenarios, CRC fallback, migration, invalid battery reads, hysteresis, SPS30 command timing and RTC interrupt ordering. These tests do not emulate an entire STM32 or RF network.

Before field deployment, verify watchdog recovery, STOP2 current, RTC rollover, missing/shorted sensors, no-gateway behavior, voltage ramps with load spikes, actual flash power cuts and decoder compatibility on the board. Run a soak test beyond the former 50-day failure point. Firmware was built and host-tested, not flashed remotely.

## Energy claims

The study's roughly 1.1-mA baseline alone represents 26.4 mAh/day. Finding its physical cause can save more than small arithmetic optimizations. Its 77-mC CO2 entry is for one shot; this firmware intentionally takes two shots per useful reading. Its SPS30 event duration also differs from the current premeasurement schedule. Reintegrate current traces for the same firmware, battery-side voltage, radio conditions and measurement cadence before claiming a percentage saving. A doubled interval means 50% fewer periodic uplinks, not 50% less total power.

Hardware sources: [BQ25185 datasheet](https://www.ti.com/lit/ds/symlink/bq25185.pdf), repository `hardware/datasheets/sps30_datasheet.pdf` (Table 8, Sleep only in Idle), `hardware/datasheets/scd4x_low_power_appnote.pdf` (discard first single shot after power-down). Original study and schematic exports are retained as historical project documents.

Local verification on 2026-09-06: Release 88,040 B ROM, Debug 168,808 B ROM, CO2-disabled Release 87,000 B ROM. All three link successfully within the reserved flash limit. Remaining three compiler warnings are existing middleware variables unused when tracing is disabled.
