# Adding a sensor (Qwiic / I2C)

PCB 1.1 has a Qwiic connector, **CN2**: a 4-pin JST-SH (1 mm) socket with GND, 3.3 V, SDA, SCL. Qwiic (SparkFun) and STEMMA QT (Adafruit) breakouts plug straight in, and further boards can be chained. Firmware 2.0 ships no driver for anything on CN2; this page shows where your own code goes. Every place is marked in the source with a comment starting `ADD YOUR SENSOR`, so `grep -rn "ADD YOUR SENSOR" stm32_node` lists them all.

## Before you plug something in

- **Same bus.** CN2 is wired to I2C2 (PA11 SDA, PA12 SCL, 4.7 kOhm pull-ups to VCC) together with the built-in parts. Used addresses: `0x36` MAX17048, `0x40` INA226, `0x44` SHT45, `0x53` LTR390, `0x62` SCD41, `0x69` SPS30, `0x77` BMP390. Your part needs a different one.
- **Always powered.** CN2's 3.3 V comes from VCC, which is never switched off. Pick a part with a sleep or one-shot mode and put it to sleep after each reading, otherwise its idle current runs the battery down. A part that has to be cut off completely belongs on 3V3SWITCHABLE (connector P4), switched with `PowerRail_3V3Sw_Activate()` / `PowerRail_3V3Sw_Deactivate()` in `Core/Src/power_rail.c`.
- **One faulty module disturbs everything.** A breakout that holds SDA low blocks all sensors; the station then reports read errors for the other parts and `0x0210` (bus recovery failed) on fPort 99. Breakouts with their own pull-ups add to the board's 4.7 kOhm; with several of them, remove theirs.

## Step by step

1. **Driver.** Add `Core/Inc/<part>.h` and `Core/Src/<part>.c`, modelled on `sht45.c`: blocking `HAL_I2C_*` calls on `hi2c2` with a timeout, return codes, values as integers in a fixed unit (e.g. 0.01 degC), never floats on the wire. Add the `.c` file to the source list in `cmake/stm32cubemx/CMakeLists.txt`.
2. **Fault id.** Add `FAULT_COMP_<PART>` in `Core/Inc/fault_codes.h`. Ids `0x0A` to `0x0F` are free; beyond that raise `FAULT_COMPONENTS` in `fault_report.h` (at most `0x20`). Add the same id with its name to the backend table (`DeviceFaultCodes.COMPONENTS` in hydronode-backend) and to `PARTS` in `doc/payload-decoder.js`, so missing parts and read errors show up with a name instead of "Code 0x01NN".
3. **Data field.** Add a field to `sensor_t` in `Core/Inc/sys_sensors.h` and a free `SENSOR_VALID_*` bit (bits 6 to 15).
4. **Init.** In `EnvSensors_Init()` (`Core/Src/sys_sensors.c`): call your init and `Fault_ComponentInit(FAULT_COMP_<PART>, found, address)`.
5. **Read.** In `EnvSensors_Read()`: read, store the value, set the valid bit, call `note_result(FAULT_COMP_<PART>, ok)`, and `I2C2_RecoverBus()` after a failure, like the blocks above it. Three failures in a row become a read-error report on fPort 99 automatically.
6. **Sleep.** In `EnvSensors_Sleep()`: put the part back to sleep.
7. **Slow parts.** A part that needs seconds to measure gets a pre-measurement timer like the SCD41 (`ScheduleNextRound()` in `LoRaWAN/App/lora_app.c`). The timer callback only posts an event; the I2C work runs in `ProcessSensorEvents()`. Never touch I2C from a timer or interrupt.
8. **Send.** Ports 2, 3 and 4 have fixed lengths that backend layouts depend on; do not change them. Give your data its own port instead, for example fPort 6 = port 2 plus your bytes: a new constant and layout in `Core/Inc/payload.h` / `Core/Src/payload.c`, selected in `Payload_Port()`. Same length on every send, missing values as `0xFFFF` (unsigned) or `0x8000` (signed), at most 51 bytes so the frame still fits DR0 to DR2 in EU868. In HydroNode, add a layout for the new fPort in the sensor's LoRaWAN settings and enter the missing value per channel.
9. **Debug output.** Print the value in `DebugProfile_Run()` (`Core/Src/debug_profile.c`) so DIP 3 shows it on H1.
10. **Check.** `python3 tests/run_tests.py`, `node tests/test_decoder.js`, `cmake --build --preset Release`. Keep the last 8 KiB of flash free (the linker map shows the size).

Scaling and conversions are easiest to get right as a small host test in `stm32_node/tests/`, like `test_payload.c`.
