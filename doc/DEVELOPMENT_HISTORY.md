# Development History

This project was developed in a private GitHub repository from January to
September 2026 and republished here as an open-source project. The original
repository's issue and pull-request threads did not carry over, so this file
preserves the development record.

The commit history itself is complete and unmodified apart from the removal of
development LoRaWAN key material and personal data (see [SECURITY.md](../SECURITY.md)).


## Issues (25)

| # | Title | Labels | Opened | Closed |
|---|---|---|---|---|
| 1 | update project structure | documentation | 2026-03-31 | 2026-04-05 |
| 2 | update README.md | documentation | 2026-03-31 | 2026-08-05 |
| 3 | add RX actions | — | 2026-03-31 | 2026-04-27 |
| 4 | Test LoRaWAN environmental strength at OTH | — | 2026-03-31 | open |
| 5 | Monitor problems/power consumption with poor to no reception | — | 2026-03-31 | 2026-08-05 |
| 6 | Implement deep sleep mode with reduced transmission frequency when battery level is low | — | 2026-03-31 | 2026-05-27 |
| 7 | implement scd41 | — | 2026-03-31 | 2026-04-16 |
| 8 | implement sps30 | — | 2026-03-31 | 2026-04-16 |
| 9 | implement MAX17048 | — | 2026-03-31 | 2026-04-14 |
| 10 | implement bme390 | — | 2026-04-01 | 2026-04-14 |
| 11 | implement sht45 bmp390 ltr390 and max17048 | — | 2026-04-01 | 2026-04-14 |
| 12 | implement LTR390 | — | 2026-04-01 | 2026-04-14 |
| 13 | implement debug interface | enhancement | 2026-04-01 | 2026-06-13 |
| 14 | design pcb | — | 2026-04-01 | 2026-04-24 |
| 16 | add gh actions | enhancement | 2026-04-05 | 2026-04-05 |
| 21 | update ioc to match | bug | 2026-04-05 | 2026-04-05 |
| 24 | update to clion and new stack | enhancement | 2026-04-09 | 2026-04-13 |
| 28 | fix ltr390 error | bug | 2026-04-14 | 2026-04-27 |
| 30 | implement helium status request | enhancement | 2026-04-15 | open |
| 36 | document project | documentation | 2026-04-21 | 2026-04-21 |
| 38 | Fan cleaning not working at init | bug | 2026-04-21 | 2026-04-23 |
| 43 | Refactor the SendTxData function to make it suitable for the HydroNode network | enhancement | 2026-04-27 | 2026-04-27 |
| 45 | test pcb | — | 2026-05-08 | 2026-05-30 |
| 46 | implement on chip uv index calc | — | 2026-05-27 | 2026-06-13 |
| 50 | project cleanup | documentation | 2026-06-13 | 2026-06-19 |

## Pull requests (31)

| # | Title | Branch | Opened | Merged |
|---|---|---|---|---|
| 15 | Withoutstmide | `withoutstmide` | 2026-04-05 | 2026-04-05 |
| 17 | add action | `16-add-gh-actions` | 2026-04-05 | 2026-04-05 |
| 18 | update makefile for actions | `16-add-gh-actions` | 2026-04-05 | 2026-04-05 |
| 19 | test | `16-add-gh-actions` | 2026-04-05 | 2026-04-05 |
| 20 | 16 add gh actions | `16-add-gh-actions` | 2026-04-05 | 2026-04-05 |
| 22 | Update Makefile and README for toolchain configuration on macOS and L… | `21-update-ioc-to-match` | 2026-04-05 | 2026-04-05 |
| 23 | add pdfs and export v1 of schematic | `21-update-ioc-to-match` | 2026-04-05 | 2026-04-09 |
| 25 | 24 update to clion and new stack | `24-update-to-clion-and-new-stack` | 2026-04-12 | 2026-04-13 |
| 26 | 11 implement sht45 | `11-implement-sht45` | 2026-04-14 | 2026-04-14 |
| 27 | Implement SCD41 power-cycled single-shot integration in `stm32_node` and uplink CO2 via CayenneLPP | `copilot/implement-scd41-sensor` | 2026-04-14 | 2026-04-14 |
| 29 | implement scd41 with new prewake logic | `7-implement-scd41-sensor` | 2026-04-15 | 2026-04-16 |
| 31 | implement sps30 and fix cayennelpp co2 sending | `8-implement-sps30` | 2026-04-16 | 2026-04-16 |
| 32 | 8 implement sps30 | `8-implement-sps30` | 2026-04-16 | 2026-04-18 |
| 33 | 2 update readmemd | `2-update-readmemd` | 2026-04-18 | — |
| 34 | 2 update readmemd | `2-update-readmemd` | 2026-04-18 | 2026-04-18 |
| 35 | finalize schem and pcb | `2-update-readmemd` | 2026-04-20 | 2026-04-20 |
| 37 | Add professional LaTeX documentation scaffold under `doc/study-documentation` | `copilot/add-documentation-structure` | 2026-04-21 | 2026-04-21 |
| 39 | 30 implement helium status request | `30-implement-helium-status-request` | 2026-04-21 | 2026-04-21 |
| 40 | update logic for sensor values; update sp30 cleaning; add rx helium cmds | `38-fan-cleaning-not-working-at-init` | 2026-04-22 | 2026-04-23 |
| 41 | 14 design pcb | `14-design-pcb` | 2026-04-24 | 2026-04-24 |
| 42 | Bug Fix: Remove conditional GPIO configuration and interrupt handling for STM3… | `14-design-pcb` | 2026-04-24 | 2026-04-24 |
| 44 | 43 refactor the sendtxdata function to make it suitable for the hydronode network | `43-refactor-the-sendtxdata-function-to-make-it-suitable-for-the-hydronode-network` | 2026-04-27 | 2026-04-27 |
| 47 | 45 test pcb | `45-test-pcb` | 2026-05-30 | 2026-05-30 |
| 48 | 45 test pcb | `45-test-pcb` | 2026-06-08 | 2026-06-08 |
| 49 | Implement various enhancements and fixes for the STM32WLE5 firmware | `45-test-pcb` | 2026-06-13 | 2026-06-13 |
| 51 | 50 project cleanup | `50-project-cleanup` | 2026-06-16 | 2026-06-19 |
| 52 | submited pdf | `50-project-cleanup` | 2026-06-19 | 2026-06-19 |
| 53 | Enhance SCD41 sensor handling and application configuration | `50-project-cleanup` | 2026-06-21 | 2026-06-21 |
| 54 | Refactor README and documentation; update LoRaWAN integration details… | `50-project-cleanup` | 2026-06-22 | 2026-06-22 |
| 55 | docs: finalize study presentation and enclosure CAD | `50-project-cleanup` | 2026-09-05 | 2026-09-05 |
| 56 | Fix 49-day uplink outage and harden unattended operation | `feature/long-term-reliability` | 2026-09-06 | 2026-09-06 |
