# HydroNodeStation01 Firmware

LoRaWAN end-node firmware for STM32WLE5 (Seeed LoRa-E5 mini target) with low-power operation and environmental telemetry.

## Project Scope

This repository currently contains two buildable applications:

- `APP=lorawan`: Full LoRaWAN end-node firmware (`LoRaWAN_End_Node`)
- `APP=lowpower`: Low-power reference firmware (`LowPower`)

The LoRaWAN application is the active project and includes sensor acquisition, payload packing, and network uplink logic.

## Hardware and Runtime Architecture

- MCU/Radio: STM32WLE5 (LoRa-E5 mini BSP)
- Radio stack: STM32WL LoRaWAN middleware + SubGHz PHY
- Sensor bus: I2C2 for external SCD41
- Low-power strategy: STOP2 enabled by default
- Build system: standalone GNU Make

## Repository Layout

Key paths:

- `Makefile`: standalone build orchestration and source discovery
- `Projects/Applications/LoRaWAN/LoRaWAN_End_Node/`: main firmware
- `Projects/Applications/LowPower/`: low-power reference app
- `Drivers/`: STM32 HAL, CMSIS, BSP
- `Middlewares/Third_Party/`: LoRaWAN and SubGHz PHY middleware
- `Utilities/`: timers, sequencer, trace, low-power helpers
- `toolchain/`: linker script and startup

## Sensor Inventory

### Confirmed In Firmware

1. SCD41 external sensor (I2C)
- CO2 (ppm)
- Relative humidity (%RH)
- Temperature (available from SCD41 driver API)

2. STM32 internal analog measurements
- Internal temperature (used in uplink payload)
- Battery voltage/level (used by LoRaWAN battery callback)

### Planned Sensor Scope From Project PDF

The final list requested from the project PDF cannot be extracted yet because the PDF is not available in this repository.

Placeholder list to complete after PDF import:

- [ ] Sensor 1 from project specification
- [ ] Sensor 2 from project specification
- [ ] Sensor 3 from project specification
- [ ] Sensor 4 from project specification

## Uplink Strategy and Payload Format

The LoRaWAN app transmits in a 3-cycle pattern:

- Cycle 0: short payload on FPort 2
- Cycle 1: short payload on FPort 2
- Cycle 2: full payload on FPort 3 (includes CO2)

Default duty cycle:

- `APP_TX_DUTYCYCLE = 60000` ms (60 s)

Payload encoding (big-endian words):

- FPort 2 (4 bytes)
  - Bytes 0..1: temperature raw (`SYS_GetTemperatureLevel() >> 8`)
  - Bytes 2..3: humidity in `% * 10`

- FPort 3 (6 bytes)
  - Bytes 0..1: temperature raw (`SYS_GetTemperatureLevel() >> 8`)
  - Bytes 2..3: humidity in `% * 10`
  - Bytes 4..5: CO2 in ppm

CO2 is measured with SCD41 single-shot mode and a wait time of 5 s before reading.

## Build and Outputs

## Prerequisites

- `arm-none-eabi-gcc` toolchain available in `PATH`, or
- STM32CubeIDE toolchain installed in the default path used by `Makefile`

## Common Commands

```bash
make                         # default build (APP=lorawan)
make APP=lorawan build       # build LoRaWAN app
make APP=lowpower build      # build LowPower app
make lorawan                 # shortcut
make lowpower                # shortcut
make size                    # print ELF size
make list                    # generate disassembly list
make bin                     # generate BIN
make clean                   # remove build artifacts
make rebuild                 # clean + build
```

## Build Artifacts

- LoRaWAN HEX: `LoRaWAN_End_Node.hex`
- LowPower HEX: `LowPower.hex`
- Intermediate outputs: `build/`

## Source Auto-Discovery

The Makefile is configured to auto-discover:

- all `*.c` files under each app's `Core/Src`
- all include directories under each app's `Core/Inc`

This means adding a new sensor module in `Core/Src` and `Core/Inc` is automatic at build level. You only need to integrate logic in application code.

## LoRaWAN Configuration

Edit these files before deployment:

1. Network and app behavior
- `Projects/Applications/LoRaWAN/LoRaWAN_End_Node/LoRaWAN/App/lora_app.h`
  - `ACTIVE_REGION`
  - `APP_TX_DUTYCYCLE`
  - LoRaWAN class and ADR defaults

2. Device identity and keys
- `Projects/Applications/LoRaWAN/LoRaWAN_End_Node/LoRaWAN/App/se-identity.h`
  - `LORAWAN_JOIN_EUI`
  - `LORAWAN_DEVICE_EUI`
  - `LORAWAN_APP_KEY`
  - `LORAWAN_NWK_KEY`

3. Sensor and low-level runtime config
- `Projects/Applications/LoRaWAN/LoRaWAN_End_Node/Core/Inc/sys_conf.h`
  - `SCD41_ENABLED`
  - I2C instance and pins
  - SCD41 timing and calibration options

## Security Guidance

- Never publish production keys in public repositories.
- Rotate keys if they were committed accidentally.
- Use environment-specific key material for dev/test/prod.

## Flashing

Typical flow:

1. Build firmware (`make APP=lorawan build`)
2. Open STM32CubeProgrammer
3. Connect via ST-LINK
4. Program `LoRaWAN_End_Node.hex`
5. Reset board and monitor UART logs

## Adding a New Sensor Module

Example: `new_sensor.c` + `new_sensor.h`

1. Place files in:
- `Core/Src/new_sensor.c`
- `Core/Inc/new_sensor.h`

2. Wire sensor logic into:
- `sys_sensors.c` / `sys_sensors.h` for read/init abstraction

3. Decide payload integration:
- add fields to short payload (FPort 2), full payload (FPort 3), or create new FPort

4. Update decoder on network-server/application side accordingly

## Validation Checklist

- Build succeeds for selected app
- Device joins LoRaWAN network (OTAA)
- Uplink interval matches `APP_TX_DUTYCYCLE`
- FPort 2 payload carries temperature + humidity
- FPort 3 payload carries temperature + humidity + CO2
- Node enters low-power mode between events

## Troubleshooting

### Build issues

- Ensure `arm-none-eabi-gcc` is installed or `TOOLCHAIN_BIN` is set.
- Run `make print-vars` to inspect selected paths and binaries.

### Join issues

- Recheck EUI/keys in `se-identity.h`.
- Confirm region setting in `lora_app.h` matches gateway/server.

### Sensor issues

- Verify I2C wiring and pull-ups for SCD41.
- Confirm SCD41 pin mapping in `sys_conf.h`.
- Check serial logs for init or single-shot measurement failures.

## Next Documentation Upgrade (After PDF Import)

When the project PDF is added, extend this README with:

1. Requirement-to-implementation traceability table
2. Final approved sensor list and electrical constraints
3. Measurement accuracy, sampling policy, and calibration process
4. Field deployment and maintenance procedures
