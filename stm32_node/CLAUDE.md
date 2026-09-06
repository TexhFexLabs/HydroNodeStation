# Project notes

Firmware 1.6 targets the STM32WLE5 custom PCB in both Release and Debug CMake presets. Build from this directory with `cmake --preset Release` then `cmake --build --preset Release`. Run host tests with `python3 tests/run_tests.py` and `node tests/test_decoder.js`.

Read `../doc/RELIABILITY.md` and `../doc/CONFIGURATION.md` for architecture, NVM migration, payload compatibility and hardware validation. Local keys belong only in the ignored `LoRaWAN/App/se-identity-local.h`; never print or commit them. Preserve the final 8 KiB of flash during normal upgrades. Do not downgrade with stale legacy nonces.

Timer IRQs only post sensor events. I2C work runs in thread mode; the shared bus remains initialized. Use MCU monotonic seconds for long deadlines, never a 32-bit millisecond value divided by 1000. Preserve STM32CubeMX USER CODE sections when modifying generated peripherals.

Follow the root user's local TODO workflow. Hardware measurements, NTC/protection wiring and field flashing require the real board; host tests do not prove years of physical reliability.
