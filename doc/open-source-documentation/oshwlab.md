# HydroNodeStation01: Open-Source Solar-Powered Environmental Monitoring Station

> **Status: Live field-testing right now.** A station is deployed outdoors and
> streaming real sensor data. Watch it live:
> **https://hydronode.texhfexlabs.de/stations/public/004dd7e7-5c27-4de0-bff5-24116f721658**

---

## Motivation

We live in an era of growing urban density, increasing climate variability, and
rising awareness of air quality as a public health issue. Yet detailed,
hyperlocal environmental data remains surprisingly scarce, most weather and air
quality stations are expensive, power-hungry, and installed only at a handful of
official sites per city. The gaps between those stations can span kilometers,
leaving entire neighborhoods without meaningful data about what they breathe
every day.

At the same time, the IoT landscape has matured enormously. Low-power wide-area
networks like LoRaWAN now make it possible to transmit sensor data over several
kilometers using only a fraction of the energy required by Wi-Fi or cellular -
often measured in microwatts during idle. Combine this with affordable solar
harvesting and modern ultra-low-power microcontrollers, and a fully
self-sufficient, unattended monitoring node becomes not just feasible, but
practical and cost-effective.

HydroNodeStation01 was born out of exactly this insight: a university
engineering project with the ambition to build a station that **anyone could
reproduce**, deploy in their garden or on a rooftop, and integrate into an open
data network, contributing to a future where environmental monitoring is dense,
democratic, and decentralized.

---

## What Works Today

This is not a paper concept. As of now, the following is built, tested, and
running:

- **All six core sensors live**: temperature, humidity, barometric pressure,
  UV index, particulate matter, and CO₂ all deliver reliable, validated readings.
- **Custom 4-layer PCB** designed from scratch in EasyEDA Pro, fabricated at
  JLCPCB, hand-assembled, and running stable.
- **End-to-end data pipeline**: the station transmits over LoRaWAN on the
  Helium network; data flows via Amazon SNS into the HydroNode backend.
- **Live dashboard + iOS app**: real-time values and historical charts for every
  sensor channel, plus station management (add, name, monitor stations).
- **Deep-sleep firmware**: STOP2 power management and a stable LoRaWAN stack with
  payload codec, send cycle, and duty-cycle handling.
- **3D-printed Stevenson screen enclosure** in UV-resistant ASA, currently in an
  outdoor field test with good results so far.
- **Automated anomaly detection** running on incoming sensor data in the backend.

Still in progress: final solar autonomy verification (see *Power System* below),
a formal AI/ML evaluation, and finishing documentation.

---

## What It Measures

The station is **not limited to a fixed set of sensors**. Thanks to the open I2C
bus architecture, virtually any compatible sensor can be added with a
corresponding firmware adaptation, making the platform fully extensible for
custom use cases and research applications. The reference design uses:

| Quantity | Sensor | Notes |
|---|---|---|
| CO₂ concentration | Sensirion **SCD41** | single-shot photoacoustic NDIR, ±40 ppm |
| Particulate matter | Sensirion **SPS30** | PM1.0 / PM2.5 / PM4 / PM10 + number concentration |
| Temperature / Humidity | Sensirion **SHT45** | ±0.1 °C / ±1.0 % RH |
| Barometric pressure | Bosch **BMP390** | ±0.5 hPa absolute |
| UV index | Lite-On **LTR390** | UVA + ambient light, UV-transparent cover |
| Battery state-of-charge | Maxim **MAX17048** | fuel gauge via I2C |

All sensors share an I2C bus and are powered through load switches, allowing the
firmware to cut power completely to idle peripherals and keep the average system
current in the single-digit microamp range between transmissions.

> **Sensor choices evolved during the project.** The reference design now uses
> SHT45 + BMP390 (more accurate and more efficient than a BME280) and the LTR390
> for UV (wider range and native I2C instead of the analog VEML6075). An ambient
> noise sensor was evaluated but dropped to keep focus on the core environmental
> channels.

---

## Hardware Design

### Microcontroller & Radio

The heart of the station is the **STM32WLE5**, a single-chip SoC that integrates
an ARM Cortex-M4 application core with a sub-GHz LoRa radio transceiver. Unlike
module-based approaches, using the bare die allows full control over the RF
front-end layout, minimizes footprint, and eliminates the markup of a pre-made
module. A custom RF path with a **BALFHB-WL-02D3** balun and **BGS12SN6** RF
switch enables both TX and RX paths to be properly matched at 868 MHz (EU).

### Power System

A **BQ25185** solar charger manages harvesting from a small panel and charges a
single-cell LiPo battery. A **TPS63900** buck-boost converter maintains a stable
3.3 V rail across the full battery discharge curve (2.5 V → 4.2 V), ensuring
consistent sensor readings and radio output power regardless of battery state. In
deep sleep (STOP2 mode), the entire system consumes well under 20 µA, enabling
multi-day autonomy even without solar input.

> **Honest note on the first board revision:** the initial PCB revision had a bug
> in the TPS63900 regulator section. Root cause was found and fixed in
> collaboration with an expert, and a corrected PCB design is already done and
> intended to drop into the already-ordered boards. In the meantime the station
> runs perfectly on a tiny workaround, the same TPS63900 chip mounted on a
> breakout board plugged into the PCB headers, with identical efficiency and
> minimal overhead. This is exactly the kind of real-world iteration anyone
> reproducing the board should expect, and it's documented openly so you don't
> repeat it.

### Power Efficiency

Efficiency is the entire point of the design, it is what makes a truly unattended,
solar-autonomous node possible. Every layer was optimized for ultra-low power:

- **Deep sleep under 20 µA** (STOP2 mode) for the complete system, MCU, radio,
  and sensors. That alone gives multi-day autonomy even with zero solar input.
- **Single-digit microamp average current** between transmissions, sensors are
  fully powered down through load switches instead of just idling.
- **LoRaWAN instead of Wi-Fi/cellular**, kilometers of range at a fraction of the
  energy, on-air power measured in microwatts during idle.
- **Single-chip STM32WLE5** (MCU + LoRa radio on one die) removes the power and
  board overhead of a separate transceiver.
- **SF7 + adaptive data rate (ADR)** minimize on-air time per transmission, which
  is the single biggest energy cost in a LoRa node.
- **Stable 3.3 V rail across the full 2.5 V→4.2 V battery curve** via the TPS63900
  buck-boost, so no energy is wasted on a linear regulator and the battery is used
  down to its last usable volt.

The result: a small solar panel and a single LiPo cell are enough to run the
station unattended for months.

### PCB

The **4-layer PCB** was designed from scratch in EasyEDA Pro, with dedicated RF,
digital, and power zones. Special attention was paid to the RF layout:
controlled-impedance traces, a solid ground plane under the antenna feed, and
proper keepout areas around the chip antenna ensure a reliable link budget over
urban LoRaWAN distances.

The PCB went through multiple iterations for RF design, antenna matching, and
component placement, by far the most time-intensive part of the whole project,
and the part where most of the open-source value sits, since no suitable existing
open design for the STM32WLE5 with this sensor set existed.

---

## Firmware & Connectivity

The firmware runs a deterministic **5-minute measurement cycle**. The station
wakes from STOP2, takes single-shot readings from all sensors, packs a compact
binary payload, and transmits via **LoRaWAN Class A** to the **Helium** network. A
`DeviceTimeReq` mechanism keeps the RTC synchronized without a dedicated GPS
module. Spreading factor SF7 is used by default to minimize on-air time and
maximize battery life, while adaptive data rate (ADR) lets the network optimize
the link automatically.

From the Helium console, data flows via **Amazon SNS** into the HydroNode
network. An **iOS app** lets users add their own sensors and weather stations and
visualizes the data, including historical charts per channel. A **public web
dashboard** shows live and historical data for every sensor, no login required
(see link at the top).

---

## Enclosure & Deployment

The electronics live in a sealed enclosure. A **3D-printed Stevenson screen** in
white UV-resistant **ASA** filament provides natural ventilation for the air
quality and temperature sensors while shielding them from direct sunlight and
rain, stacked louvers block sun and rain but let air circulate freely. This
matters: for correct outdoor temperature and humidity readings the housing must
not heat up under direct sun. Printing was done on a 3D printer acquired
specifically for the project.

The station is designed to be mounted on a standard pole or balcony railing and
left unattended for months. A waterproof cable gland handles the solar panel
lead.

> **Open detail, UV-transparent window:** the LTR390 UV sensor needs protection
> while still seeing UV light. Ordinary glass and PETG block much of the UV and
> falsify readings. Proper UV-transparent quartz/borosilicate glass runs ~€50 with
> shipping; cheaper UV-A/UV-B-transparent films and optical materials are
> currently being tested. If you reproduce this, plan for the window material.

---

## Attached Design Files

Everything you need to reproduce the station is attached to this project:

- **Schematic & PCB**: full EasyEDA Pro project.
- **`*.3mf`**: the 3D-printable **Stevenson screen** for the sensor head.
- **`*.3mf`**: the 3D-printable **enclosure for the PCB**.

EasyEDA being browser-based means you can fork the schematic and board, swap
parts, and order from JLCPCB directly, no desktop EDA install needed.

---

## Reproduction Cost

Total cost depends heavily on the chosen sensor configuration. The design is
**modular**: not every sensor needs to be populated:

- **Minimal build** (temperature + humidity + pressure, e.g. substituting an
  SHT30 for the SHT45): well under **$30** in additional parts.
- **Fully populated** station with all sensors: roughly **$120–150**.

This flexibility makes the design accessible across very different budgets and use
cases, from a single hobbyist node to a research-grade air-quality array.

---

## Open Source

All hardware design files (EasyEDA schematic & PCB) plus the 3D enclosure models
are published openly under **CC BY-NC-SA 4.0**. The goal is to make this station
reproducible by anyone with basic soldering skills and access to a PCB fabrication
service, at a BOM cost that scales with the sensor configuration chosen.

If you want to build dense, hyperlocal environmental monitoring in your own area:
fork it, build it, deploy it, and join the network. Live data is already flowing
at **hydronode.texhfexlabs.de**.
