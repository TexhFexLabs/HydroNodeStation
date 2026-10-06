# Project notes

Firmware 2.0 targets PCB 1.1 (hardware revision 2) in both Release and Debug CMake presets; revision 1 boards stay on firmware 1.6.x. Build from this directory with `cmake --preset Release` then `cmake --build --preset Release`. Run host tests with `python3 tests/run_tests.py` and `node tests/test_decoder.js`.

Read `../doc/RELIABILITY.md` and `../doc/CONFIGURATION.md` for architecture, NVM migration, payloads (ports 2/3 with block, fault frames on fPort 99, codes in `Core/Inc/fault_codes.h`) and hardware validation. Local keys belong only in the ignored `LoRaWAN/App/se-identity-local.h`; never print or commit them. Preserve the final 8 KiB of flash during normal upgrades. Do not downgrade with stale legacy nonces.

Timer IRQs do no bus I/O: sensor timers only post events, I2C work runs in thread mode, and the shared bus remains initialized. Driving a GPIO from a timer callback is fine (LED patterns in `led.c`). Use MCU monotonic seconds for long deadlines, never a 32-bit millisecond value divided by 1000. Preserve STM32CubeMX USER CODE sections when modifying generated peripherals.

`LoRaWAN_End_Node_LBM.ioc` is frozen: it is still the ST template and does not match the board (clocks, pins, peripherals). Never open it in STM32CubeMX to regenerate code, that would overwrite the pin map and the clock setup. The generated code is maintained by hand; `Core/Inc/main.h`, `Core/Src/gpio.c` and the radio BSP for PCB 1.1 are authoritative.

Hardware measurements, NTC/protection wiring and field flashing require the real board; host tests do not prove years of physical reliability. Open work is tracked in the repository's GitHub issues.
