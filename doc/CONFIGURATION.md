# Configuration reference (firmware 1.6)

For fault recovery, migration and hardware test requirements see [RELIABILITY.md](RELIABILITY.md).

Deutsche Backend-Übergabe einschließlich Fehlerwerte, TTN-Webhooks, Testvektoren und Kalibrierung: [BACKEND_UMBAU_UND_KALIBRIERUNG.md](BACKEND_UMBAU_UND_KALIBRIERUNG.md).

## Credentials

Put production credentials in `stm32_node/LoRaWAN/App/se-identity-local.h`, which is ignored by Git. Use the same four `LORAWAN_DEVICE_EUI`, `LORAWAN_JOIN_EUI`, `LORAWAN_APP_KEY`, and `LORAWAN_GEN_APP_KEY` macro names as in `se-identity.h`. Do not edit the tracked default header or use skip-worktree to hide production keys. Byte lists use comma-separated hexadecimal tokens without `0x`, as in the default header. An all-zero DevEUI selects the silicon-derived ID; a nonzero one is respected.

## Build flags

`Core/Inc/sys_conf.h`: `APP_LOG_ENABLED=0` and `DEBUGGER_ENABLED=0` are production defaults. `LOW_POWER_DISABLE=0` enables STOP2. `SCD41_ENABLED=0` now actually excludes CO2 work. Enable logging explicitly when using the PB4 diagnostic profile. Both current CMake builds target STM32WLE5; the old WL55/prototype documentation is historical.

## Timing and energy

| Setting | Default | Location |
|---|---|---|
| Uplink interval | 180 seconds; downlink-configurable 30–3600 | `LoRaWAN/App/lora_app.h` |
| CO2 cadence | Every fifth uplink | `lora_app.c` |
| PM cadence | Every tenth uplink | `lora_app.c` |
| SCD41 first/useful shot | 12 / 6 seconds before uplink | `lora_app.h` |
| SPS30 start | 16.5 seconds before uplink | `lora_app.h` |
| SAVE entry / exit | 3500 / 3650 mV; twice the interval | `Core/Inc/power_policy.h` |
| RECOVERY entry / exit | 3300 / 3600 mV; stable recovery for 60 s | `power_policy.h` |
| Battery checks | 60 seconds | `power_policy.h` |
| Maximum STOP2 sleep | 8 seconds for independent watchdog | `Core/Inc/runtime_health.h` |
| Charger CE pulse | Every 2 hours | `lora_app.c` |

Battery thresholds require validation against the installed cell. RECOVERY cancels radio and heavy sensor work and resumes after sustained voltage recovery. It does not replace hardware cell protection or an NTC.

## Downlinks (fPort 2)

| Payload | Action |
|---|---|
| `10 HH LL` | Set interval in seconds, 30–3600; apply after successful persistence, effective next scheduling cycle |
| `11` | SPS30 cleaning when battery state is NORMAL |
| `12` | Request diagnostic response on fPort 5 |
| `FF` | Software reset |

## Payloads

All fields are two-byte big-endian. Temperature is **signed**, all others unsigned. Missing temperature is `0x8000`; other missing values are `0xFFFF`. New decoder: [payload-decoder.js](payload-decoder.js).

| fPort | Bytes | Fields |
|---|---|---|
| 2 | 10 | T /100 °C, RH /100 %, pressure /10 hPa, battery mV, UV /100 |
| 3 | 12 | Port 2 + CO2 ppm |
| 4 | 32 | Port 3 + PM1/2.5/4/10 /10 µg/m³, NC0.5/1/2.5/4/10 /10 #/cm³, typical size /1000 µm |
| 5 | 22 | Versioned diagnostics; layout in RELIABILITY.md |

Port 4 retains its CO2 slot as missing when CO2 is disabled. Port 5 is optional and does not alter measurement port layouts. Validate the separate backend's sentinel/diagnostic handling before field rollout.

RF region is selected by `ACTIVE_REGION` in `lora_app.h`; the compiled region set is in `LoRaWAN/Target/lorawan_conf.h`.
