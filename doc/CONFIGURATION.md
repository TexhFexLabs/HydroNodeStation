# Configuration reference (firmware 2.0, PCB 1.1)

Firmware 2.0 targets hardware revision 2 (PCB 1.1). Boards of revision 1 stay on firmware 1.6.x: the pin map differs (charger CE, RF switch supply, switched rails, INA226) and 2.0 does not run on them.

For fault recovery, migration and hardware test requirements see [RELIABILITY.md](RELIABILITY.md).

## Credentials

Put production credentials in `stm32_node/LoRaWAN/App/se-identity-local.h`, which is ignored by Git. Use the same four `LORAWAN_DEVICE_EUI`, `LORAWAN_JOIN_EUI`, `LORAWAN_APP_KEY`, and `LORAWAN_GEN_APP_KEY` macro names as in `se-identity.h`. Do not edit the tracked default header or use skip-worktree to hide production keys. Byte lists use comma-separated hexadecimal tokens without `0x`, as in the default header. An all-zero DevEUI selects the silicon-derived ID; a nonzero one is respected.

## Build flags

`Core/Inc/sys_conf.h`:

| Flag | Release | Meaning |
|---|---|---|
| `APP_LOG_ENABLED` | 0 | Trace over USART1 (H1, PB6/PB7). The Debug preset sets 1. |
| `DEBUGGER_ENABLED` | 0 | Keeps SWD alive in STOP2. Never routes probe lines onto board pins. |
| `LOW_POWER_DISABLE` | 0 | 0 enables STOP2. |
| `SCD41_ENABLED` | 1 | 0 excludes all CO2 work; port 3 is then never sent. |
| `PULSE_COUNTERS_ENABLED` | 0 | 1 counts contacts on PA4 and PA5 (see below). 0 keeps the pins analog and sends `0xFFFF` in both counter fields. |

Both CMake presets (Release, Debug) target the STM32WLE5 on PCB 1.1. `stm32_node.ioc` is frozen and must not be regenerated, see `stm32_node/CLAUDE.md`.

## Pins (PCB 1.1)

| Net | Pin | Use |
|---|---|---|
| `EN5V` | PA9 | 5 V rail (SPS30). On from boot, stays on; the SPS30 sleeps over I2C. |
| `EN3V3SW` | PA8 | Switchable 3.3 V rail. Off; nothing on this station uses it. |
| `CHG_CE` | PA2 | BQ25185 CE. Analog = charging (R7 pulls low), push-pull high for the 500 ms pulse every 2 h. |
| `RF_SW_VDD` / `RF_SW_CTRL` | PB12 / PC13 | RF switch supply and select, see RELIABILITY.md. PB0 (VDD_TCXO) is never driven. |
| `SOLAR_ADC` | PB2 (ADC_IN4) | Panel voltage through 150 k / 100 k, factor 2.5. |
| `MAX17048_ALRT` | PA0 | Analog, not used (gauge runs in hibernate). |
| `CNT1` / `CNT2` | PA4 / PA5 | Contact counters, only with `PULSE_COUNTERS_ENABLED`. |
| `DIP3_DEBUG` / `DIP4_INSTALL` | PB4 / PB8 | Read once at boot, active low. |
| `LED1` / `LED_DIAG` | PB5 / PB3 | Low-active, visible only with DIP 1 / DIP 2 closed. |
| `DBG_UART` | PB6 / PB7 | USART1 on H1, only while the debug profile or a trace writes. |

I2C2 on PA11 (SDA) / PA12 (SCL) carries SHT45 0x44, BMP390 0x77, LTR390 0x53, SCD41 0x62, SPS30 0x69, MAX17048 0x36 and INA226 0x40. Unused pins are analog.

## Timing and energy

| Setting | Default | Location |
|---|---|---|
| Uplink interval | 180 s, downlink `10 HH LL` sets 30 to 3600 s and applies at once | `LoRaWAN/App/lora_app.h` |
| SCD41 first / useful shot | 12 / 6 s before the uplink | `lora_app.h` |
| SPS30 start | 16.5 s before the uplink | `lora_app.h` |
| Solar sample (INA226 + ADC) | every 30 s in NORMAL and SAVE | `lora_app.c` |
| MAX17048 | hibernate, measures every 45 s on its own | `max17048.c` |
| Link check | 24 h after the join, then daily; rejoin after 3 unanswered days | `Core/Inc/link_check.h` |
| Charger CE pulse | 500 ms every 2 h | `lora_app.c` |
| Maximum STOP2 sleep | 8 s for the independent watchdog | `Core/Inc/runtime_health.h` |

### Power modes

The measurement plan follows the battery. Thresholds are in `Core/Inc/power_policy.h` and are starting values to validate against the installed cell and its protection circuit.

| Mode | Enter | Leave | Interval | CO2 | PM | Radio |
|---|---|---|---|---|---|---|
| NORMAL | | | 180 s | every 5th round | every 10th round | yes |
| SAVE | below 3500 mV | 3650 mV | 360 s | every 10th round (hourly) | no | yes |
| RECOVERY | below 3300 mV or 3 invalid readings | 3600 mV stable for 60 s | none | no | no | off |
| Standby | in RECOVERY, 2 valid readings in a row below 3100 mV | wake-up every 60 min, boot when at least 3600 mV | none | no | no | off |

The battery is checked every 60 s. An invalid reading never leads into Standby: a missing measurement is no undervoltage. Mode changes are reported on fPort 99 (`0x0320`/`0x8320` SAVE, `0x0321`/`0x8321` RECOVERY). RECOVERY entry and exit leave together once the radio is back.

Standby turns off everything except VCC, the RTC with its LSE and the fuel gauge. The INA226 is shut down, the RF switch is off, and the EN pins float so the 1 MΩ pull-downs switch off the 5 V and 3V3SWITCHABLE rails. Waking up is a reset: the boot sees `PWR_FLAG_SB`, reads only the MAX17048, and goes back to Standby below 3600 mV. The full boot afterwards reports `0x0540` with reason 8.

**Option byte IWDG_STDBY.** The independent watchdog must be frozen in Standby, otherwise it resets the MCU after a few seconds. Clear `IWDG_STDBY` once when setting up a board, with STM32CubeProgrammer (Option bytes, User configuration, uncheck IWDG_STDBY) or `STM32_Programmer_CLI -c port=SWD -ob IWDG_STDBY=0`. The firmware checks `FLASH->OPTR` at every boot. Without the setting the station never enters Standby, stays in RECOVERY, and reports `0x0544` at every boot.

## DIP switches

| DIP | Function |
|---|---|
| 1 | Connects LED1 (PB5). Open: LED1 stays dark whatever the firmware does. |
| 2 | Connects LED_DIAG (PB3). |
| 3 | Debug profile: no LoRaWAN, all sensors (with INA226 V/I/P, solar ADC, SOC, charge rate, board temperature and counters) read in a loop and printed over USART1 on H1, 115200 baud. Wins over DIP 4. |
| 4 | Installation mode: after the first join, 30 min of 60-second uplinks on port 2 (the modem keeps the duty cycle), a link check after every 3rd accepted uplink, at most 10 per run. Normal operation afterwards until the next boot. |

DIP 3 and 4 are read once at boot with a short internal pull-up (closed = low); the pins are analog afterwards, so a closed switch draws no current.

## LEDs

| Phase | LED1 (DIP 1) | LED_DIAG (DIP 2) |
|---|---|---|
| Boot until the first uplink | short flash per join attempt, double flash on join, one flash for the first uplink; dark at the latest 30 min after boot | |
| Installation mode | 50 ms flash per accepted uplink; link check answer: one flash per gateway (max. 5), no answer: one long flash (1 s) | |
| Fault | | reason, pause, failing step (see RELIABILITY.md); reason 6 (LSE) and 8 (before Standby) as well |

Outside these patterns both pins are analog.

## Contact counters

With `PULSE_COUNTERS_ENABLED=1`, PA4 (counter 1, rain gauge, 50 ms lockout) and PA5 (counter 2, anemometer, 2 ms lockout) count falling edges through EXTI without internal pull-ups. LPTIM2 would count without waking up, but it does not run in Stop 2, so both channels use EXTI. The board's RC filter (1 MΩ / 1 nF) is meant for contacts up to about 50 Hz; fast signals are not supported. Counts are reported per block (see below) and saturate at `0xFFFE`.

## Downlinks (fPort 2)

| Payload | Action |
|---|---|
| `10 HH LL` | Set the interval in seconds, 30 to 3600. Stored in flash, then the next round is scheduled from now. |
| `11` | SPS30 fan cleaning, only in NORMAL with a valid battery reading |
| `12` | Diagnostic response on fPort 5 |
| `FF` | Restart. Stores reason 7 first; the acknowledgement is the `0x0540` entry with reason 7 after the boot. |

Every non-empty downlink on fPort 2 is acknowledged on fPort 99 as code `0x06CC` (CC = command byte) with the result in the detail. Empty downlinks and other ports are ignored without an acknowledgement. The acknowledgement leaves with the next fPort 99 frame, not at once.

| Result | Meaning |
|---|---|
| 0 | executed (`10`: the new interval is in effect) |
| 1 | length or parameter invalid |
| 2 | refused because of battery or power mode |
| 3 | could not be saved (`10`: the interval stays unchanged) |
| 4 | unknown command |
| 5 | the modem did not accept the reply (`12`) |
| 6 | execution failed (`11`: SPS30 did not respond or cleaning did not start) |

## Payloads

All fields are big-endian. Every port has one fixed length. Missing unsigned values are `0xFFFF`, missing signed values `0x8000`. Decoder: [payload-decoder.js](payload-decoder.js).

| fPort | Bytes | Content |
|---|---|---|
| 2 | 32 | Base (10) + block (22) |
| 3 | 34 | Base (10) + CO2 (2) + block (22) |
| 4 | 32 | Base + CO2 + PM, no block (as in 1.x) |
| 5 | 26 | Diagnostics on request, layout in RELIABILITY.md |
| 99 | 4 to 32 | Faults, events, command acknowledgements |

Port 4 carries no block: 54 bytes would not fit DR0 to DR2 (51 bytes in EU868). Its block values flow into the next port 2 or 3 frame.

**Base.** Temperature i16 0.01 °C, relative humidity u16 0.01 %, pressure u16 0.1 hPa, battery u16 mV, UV index u16 0.01. CO2 u16 ppm. Port 4 continues with PM1/2.5/4/10 u16 0.1 µg/m³, NC0.5/1/2.5/4/10 u16 0.1 #/cm³ and typical particle size u16 0.001 µm. Port 4 keeps its CO2 slot as missing when CO2 is disabled.

**Block** (offset from the start of the block):

| Off | Field | Type | Unit | Missing |
|---|---|---|---|---|
| 0 | Solar voltage INA226, mean | u16 | mV | `0xFFFF` |
| 2 | Solar current INA226, mean | u16 | 0.1 mA | `0xFFFF` |
| 4 | Solar power, mean | u16 | mW | `0xFFFF` |
| 6 | Solar energy since the last block | u16 | 0.1 mWh | `0xFFFF` |
| 8 | Sunshine time since the last block | u16 | s | `0xFFFF` |
| 10 | Solar voltage ADC PB2, mean | u16 | mV | `0xFFFF` |
| 12 | State of charge | u16 | 0.01 % | `0xFFFF` |
| 14 | Charge rate | i16 | 0.01 %/h | `0x8000` |
| 16 | Board temperature | i16 | 0.01 °C | `0x8000` |
| 18 | Counter 1 (PA4) since the last block | u16 | pulses | `0xFFFF` = off |
| 20 | Counter 2 (PA5) since the last block | u16 | pulses | `0xFFFF` = off |

"Since the last block" means since the last block the modem accepted. Sums saturate at `0xFFFE`. Sunshine counts a 30-second sample when the panel delivers more than 68 mW or the ADC sees more than 5.5 V. Backend layouts may leave the counters out; surplus bytes are ignored.

Firmware 1.x frames (port 2 with 10 bytes, port 3 with 12) still decode without the block, so a station keeping its DevEUI stays readable during the swap. Port 2 grows from 10 to 32 bytes; at SF12 an uplink takes about 1.5 s instead of 1.0 s, so the TTN fair use (30 s uplink airtime per day) gets tight at 180 s on poor links.

## Faults and events (fPort 99)

Unconfirmed uplink, 1 to 8 entries of 4 bytes: code u16 + detail u16. Code bit 15 = resolved (1) or occurred (0), bits 14..8 = category, bits 7..0 = part or event. The table is `stm32_node/Core/Inc/fault_codes.h`; codes are wire format and never renumbered.

| Category | Meaning | Bit 15 | Detail |
|---|---|---|---|
| `0x01` | Part not found at start | yes | 7-bit I2C address, otherwise 0. Resolved: 0 |
| `0x02` | Read errors in operation (3 in a row) | yes | HAL I2C error bits of the last attempt, otherwise 0. Resolved: failed attempts, saturating |
| `0x03` | Supply | yes | battery mV, `0xFFFF` unknown |
| `0x04` | Radio / LoRaWAN | no | see below |
| `0x05` | System | no | see below |
| `0x06` | Command acknowledgement | no | result, see Downlinks |

Parts for `0x01`/`0x02`: `0x01` SHT45, `0x02` BMP390, `0x03` LTR390, `0x04` SCD41, `0x05` SPS30, `0x06` MAX17048, `0x07` INA226, `0x08` solar ADC, `0x09` internal temperature sensor, `0x10` I2C bus (recovery failed).

| Code | Event | Detail |
|---|---|---|
| `0x0320` | SAVE | battery mV |
| `0x0321` | RECOVERY | battery mV |
| `0x0323` | Battery reading invalid | battery mV or `0xFFFF` |
| `0x0430` | Rejoined after the link check | days without an answer |
| `0x0431` | Uplink not sent (TX done "not sent") | count since boot |
| `0x0432` | Uplink request rejected (not BUSY/NO_TIME) | count since boot |
| `0x0540` | Restart | high byte: reason, low byte: reset flags |
| `0x0541` | Firmware version | major << 8 \| minor, `0x0200` |
| `0x0542` | LSE started in the second attempt | attempts |
| `0x0543` | NVM error | last NVM error code |
| `0x0544` | Option byte IWDG_STDBY not cleared, no Standby | 0 |
| `0x054F` | Queue overflow | dropped entries |

Restart reasons (high byte of `0x0540`): 0 none, 1 HAL, 2 CPU, 3 modem, 4 missing progress, 5 NVM, 6 LSE, 7 command `FF`, 8 Standby wake-up. Reset flags (low byte): bit 0 pin, 1 brown-out, 2 software, 3 IWDG, 4 WWDG, 5 low-power, 6 option byte, bit 7 free.

Examples: `0102 0077` = BMP390 not found at start; `0205 0004` = SPS30 read errors (NACK); `8205 0007` = SPS30 reads again after 7 failed attempts; `0540 0001` + `0541 0200` = boot after a pin reset, firmware 2.0.

**Sending.** States (`0x01`-`0x03`) are reported once on entry and once on exit; repeated errors while a state is active add nothing. Events are reported per occurrence. After every boot, once after the join, `0x0540` and `0x0541` leave in one frame. A RAM queue holds 16 entries; at most one fPort 99 frame per round, right after the accepted regular uplink. The rest follows in the next round. On overflow the oldest resolved entries go first, then the oldest; `0x054F` reports the loss. Nothing survives a reset except the restart reason (RTC backup DR6/DR7). Each frame costs one extra uplink, and only when something happened.

The HydroNode backend shows every entry in the device console, keeps open states in the monitoring status and can mail them (off by default).

RF region is selected by `ACTIVE_REGION` in `lora_app.h`; the compiled region set is in `LoRaWAN/Target/lorawan_conf.h`.
