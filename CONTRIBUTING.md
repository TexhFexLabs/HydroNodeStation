# Contributing to HydroNodeStation

Thanks for looking. This is an open-source environmental monitoring node, and it
gets better with every station that actually goes outside and reports back.

## The most valuable contributions

You do not need to write firmware to help.

1. **Field reports.** Build a station, deploy it, and open an issue with your
   location, sensor configuration, enclosure variant, and what failed. Real
   deployment data beats code.
2. **The 1.1 mA leakage hunt.** Measured day-average baseline draw is ~1.1 mA
   against a ~5.7 µA datasheet floor. Suspects: SCD41 IR leakage, BQ25185
   quiescent current, PCB leakage paths. This is the highest-impact open problem
   on the project — see the README power budget section.
3. **A UV-transparent window under €50.** The LTR390 needs UV through the
   enclosure; glass and PETG block it. Cheap alternatives that actually pass
   UV-A/UV-B are wanted.
4. **Sensor drivers** for additional I2C devices — soil moisture, VOC, noise,
   water level. The bus architecture is deliberately open-ended.
5. **Enclosure improvements** — sealing, print reliability, alternative mounts.

## Before you start

Open an issue first for anything non-trivial. It is much cheaper to agree on an
approach than to review a large PR that took the project somewhere it was not
going.

Issues labelled [`good first issue`](../../issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)
and [`help wanted`](../../issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22)
are the easiest way in.

## Firmware changes

Both build targets must compile:

```bash
cd stm32_node

# Release — custom PCB, STM32WLE5CCU6
cmake -B build/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Release

# Debug — Wio-E5 mini prototyping board
cmake -B build/Debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Debug
```

GitHub Actions checks both targets on every push.

**Guidelines:**

- Keep changes to STM32CubeMX-generated files inside the
  `/* USER CODE BEGIN */ … /* USER CODE END */` blocks, or the next `.ioc`
  regeneration will silently delete them.
- Sensor drivers live in `stm32_node/Core/Src/` and follow the existing shape:
  no blocking waits longer than necessary, explicit error returns, no direct
  HAL calls outside the driver.
- Host-side regression tests are in `stm32_node/tests/`. Add one for anything
  with non-trivial arithmetic — the timebase overflow bug that firmware 1.6
  fixes was found by exactly such a test.
- **Never** commit real LoRaWAN keys. See [SECURITY.md](SECURITY.md).

## Hardware changes

The design lives in EasyEDA Pro and is published on
[OSHWLab](https://oshwlab.com/knollfelix004/project_fegzdygg). Fork it there,
make your change, and link your forked project in the issue or PR. Export the
updated schematic/PCB PDFs into `hardware/` so the change is reviewable without
an EasyEDA account.

Hardware modifications you distribute must be released under CERN-OHL-S v2 —
see [LICENSING.md](LICENSING.md).

## Documentation changes

Documentation is CC BY-SA 4.0. English is preferred for anything new; the
existing German university reports in `doc/study-documentation/` and
`doc/zwischenbericht/` stay as they are — they are historical records, not
living documentation.

## Commits and pull requests

- Keep commits focused; one logical change per commit.
- Use imperative subject lines (`fix: reject incomplete SCD41 sequences`).
- Describe *why*, not just *what*, in the body when the reason is not obvious.
- Reference the issue you are closing.

## Licensing of contributions

By submitting a contribution you agree that it is licensed under the terms that
already apply to the part of the tree you touched — CERN-OHL-S v2 for hardware,
CC BY-SA 4.0 for documentation, MIT for own firmware code. See
[LICENSING.md](LICENSING.md).

## Code of conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md). In short:
assume good faith, critique the work and not the person, and accept that
maintainer time is limited and unpaid. Harassment of any kind means you are out.
