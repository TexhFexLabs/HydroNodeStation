# HydroNodeStation01 — Solar-Autonomous Open-Source Environmental Monitoring Node (STM32WLE5 + LoRaWAN)

> **[IMAGE 1 — COVER]** Finished station mounted outdoors on the pole, Stevenson
> screen and solar panel visible, daylight. *This image must be set as the project
> cover.*

**A fully solar-autonomous, six-channel environmental sensor node built around a
bare STM32WLE5 die (MCU + LoRa radio on one chip). Designed from scratch in
EasyEDA Pro, deployed outdoors, streaming live data right now — no login
required:**

### ▶ **[Live public dashboard](https://hydronode.texhfexlabs.de/stations/public/004dd7e7-5c27-4de0-bff5-24116f721658)**

| | |
|---|---|
| **Status** | Built, assembled, deployed, field-tested — **live data flowing today** |
| **Design tool** | 100 % EasyEDA Pro (schematic + 4-layer PCB) |
| **Fabrication** | JLCPCB (PCB + partial SMT assembly) |
| **Radio** | LoRaWAN Class A, EU868, Helium network, LoRa Basics Modem |
| **Measurement cycle** | 3 min, remotely adjustable over LoRaWAN downlink (30–3600 s) |
| **Battery life (measured)** | **~24 days** on a 1200 mAh LiPo with **zero** solar input |
| **Reproduction cost** | **~$30** minimal build · **~$120–150** fully populated |
| **Licenses** | Hardware **CERN-OHL-S v2** · Docs **CC BY-SA 4.0** · Firmware **MIT** |

> **[VIDEO]** ▶ **Demo video:** `<YOUTUBE-LINK-HIER-EINFÜGEN>`
> *(full build, deployment and live-data walkthrough)*

---

## 1. The Problem This Solves

Air quality is a public-health issue, and climate variability is increasing — yet
**hyperlocal environmental data is still scarce.** Official monitoring stations
are expensive, mains-powered, and installed at only a handful of sites per city.
The gaps between them span kilometres. Entire neighbourhoods have no meaningful
data about the air they breathe.

The blocker has never been sensors. It is **power and connectivity**. A station
that needs mains power or Wi-Fi can only go where infrastructure already exists —
which is exactly where the data gaps are not.

**HydroNodeStation01 removes that blocker.** It is a monitoring node that needs
no mains, no Wi-Fi, no cellular contract, and no maintenance visits. Put it on a
pole, a rooftop, a balcony railing, or a fence in the middle of a field. It runs
unattended and reports every three minutes over a kilometres-long LoRaWAN
link.

The design goal was never "a weather station". It was: **a station that anyone
can reproduce, at a cost that scales with their budget, so that dense
environmental monitoring becomes something a community can build for itself.**

---

## 2. What Makes It Different

This is the part that did not exist before this project. Each point below was a
deliberate engineering decision, not a default.

### 2.1 Bare STM32WLE5 die, not a pre-made LoRa module

Almost every open-source LoRaWAN node uses a certified radio module (RAK, Seeed
E5, RFM95 + separate MCU). That is the easy path — and it costs you the RF
front-end, the footprint, and the module markup.

This design places the **bare STM32WLE5 SoC** on the board and builds the RF
chain from scratch: a **BALFHB-WL-02D3** integrated balun and a **BGS12SN6** RF
switch give properly matched, separate TX and RX paths at 868 MHz. That means
full control of impedance, ground return, and antenna feed — and a node that is
smaller and cheaper than any module-based equivalent.

**At the time of design, no suitable open-source STM32WLE5 reference design with
this sensor set existed.** That gap is the core open-source contribution of this
project.

### 2.2 Staggered pre-wakeup, so the MCU never busy-waits

The two interesting sensors are slow. The SCD41 needs ~5 s per single-shot
measurement, the SPS30 needs fan spin-up plus settling. A naive implementation
keeps the MCU awake through all of it — which is where the energy actually goes.

Instead the firmware **schedules each slow sensor backwards from the transmit
window** and sleeps through their measurement time:

- The **SCD41** runs *two* power-cycled single shots: a throw-away
  **stabilisation** shot starting 12 s before the uplink, then the **useful**
  shot starting 6 s before it. Only the second is transmitted — the first exists
  purely to settle the photoacoustic cell after power-up. Without it, the first
  reading after a power cycle is measurably wrong.
- The **SPS30** starts 16.5 s before the uplink to cover fan spin-up and
  particle settling.

The MCU is in STOP2 for essentially all of that time. This is the kind of detail
that does not show up in a block diagram but decides whether a node lasts weeks
or days.

### 2.3 Buck-boost, not LDO

A **TPS63900** buck-boost holds a stable 3.3 V rail across the *entire* battery
curve, 2.5 V → 4.2 V. Two consequences that matter:

- **Sensor accuracy stays constant** and radio output power stays constant
  regardless of battery state — an LDO-based node slowly degrades as the cell
  drains.
- **The battery is used down to its last usable volt.** No energy is burned as
  heat in a linear regulator.

### 2.4 Sensor-agnostic by architecture

All sensors sit on a shared I²C bus and are commanded into their own
power-down states between measurements. **The reference sensor set is a starting
point, not a limit.** Any I²C sensor can be added with a firmware adaptation —
soil moisture, VOC, noise, radiation, water level. The platform is an extensible
research instrument, not a fixed product.

### 2.5 Remote control without a site visit

An unattended node in the field is only maintainable if you can change its
behaviour over the air. The firmware accepts LoRaWAN downlink commands:

| Command | Effect |
|---|---|
| `0x10 HH LL` | Set the TX interval to `HHLL` seconds (30–3600), persisted to flash |
| `0x11` | Trigger an SPS30 fan-cleaning cycle |
| `0x12` | Request boot / reset / sensor diagnostics on fPort 5 |
| `0xFF` | Software reset |

Combined with the transactional NVM layer, a station can be re-tuned, diagnosed,
and recovered without anyone climbing to it. The SPS30 additionally
self-cleans every 120 h, but only when the battery is above 4120 mV — maintenance
never gets to brown out the node.

---

## 3. What It Measures

> **[IMAGE 2]** Assembled PCB, top side, sensors populated, macro shot.

| Quantity | Sensor | Accuracy / Notes |
|---|---|---|
| CO₂ concentration | Sensirion **SCD41** | Single-shot photoacoustic NDIR, ±40 ppm |
| Particulate matter | Sensirion **SPS30** | PM1.0 / PM2.5 / PM4 / PM10 + number concentration |
| Temperature / Humidity | Sensirion **SHT45** | ±0.1 °C / ±1.0 % RH |
| Barometric pressure | Bosch **BMP390** | ±0.5 hPa absolute |
| UV index | Lite-On **LTR390** | UVA + ambient light, needs UV-transparent window |
| Battery state of charge | Maxim **MAX17048** | I²C fuel gauge, ModelGauge — no sense resistor |

**All six channels are live and validated in the field.** This is measured
hardware, not a BOM wish list.

> **Design evolution — documented openly.** The sensor set changed during
> development. The original BME280 was replaced by **SHT45 + BMP390** (better
> accuracy *and* lower power at a similar cost). The analog **VEML6075** was
> replaced by the **LTR390** (wider range, native I²C, no ADC channel burned). An
> ambient-noise channel was evaluated and deliberately dropped to keep the power
> budget honest. If you reproduce this board, you are getting the *second*
> iteration of these decisions, not the first guess.

---

## 4. Hardware Design

> **[IMAGE 3]** EasyEDA Pro 3D render of the 4-layer PCB (top + bottom).

### 4.1 PCB

- **4 layers**, designed from scratch in **EasyEDA Pro**.
- Strictly separated **RF / digital / power zones** to keep switching noise out
  of the analog sensor front-ends and the radio.
- **Controlled-impedance 50 Ω traces** on the antenna feed, with a solid,
  uninterrupted ground plane directly beneath and stitching vias along the RF
  path.
- **Keep-out zones** around the chip antenna respected per the manufacturer's
  reference layout — the single most common failure in DIY LoRa boards.
- Test points on every rail and on the SWD/UART lines, because a board you cannot
  probe is a board you cannot debug.

The RF layout and antenna matching were **by far the most time-intensive part of
the entire project**, and are where most of the reusable open-source value sits.
Anyone forking this board inherits an RF section that has been iterated and
measured, not copied from a datasheet diagram.

### 4.2 Power path

> **[IMAGE 4]** Enclosure open, PCB installed, wiring and solar-panel cable
> gland visible.

```
Solar panel ──► BQ25185 ──► 1S LiPo ──► TPS63900 ──► 3.3 V rail
              (solar charge     (2.5–4.2 V)   (buck-boost)     │
               controller)                                     ├─► STM32WLE5 + RF
                                                               ├─► I²C sensor bus
                                                               └─► SPS30 (high-current PM fan)
```

- **BQ25185** solar charge controller — harvests from a small panel, handles the
  full LiPo charge profile, and keeps the system running while charging.
- **TPS63900** buck-boost — the stable-rail argument from §2.3.
- **MAX17048** fuel gauge — real state-of-charge telemetry sent with every packet,
  so an operator can see a node's energy budget remotely instead of guessing.
- Sensors are commanded into their sleep/power-down states between measurements;
  how completely a rail is isolated depends on the board wiring, which is exactly
  what the open measurement below is about.

### 4.3 Measured power budget — and one unsolved problem

These are **PPK2 measurements on the actual custom PCB**, not datasheet
arithmetic. They are published here in full, including the part that does not
look good yet.

| Source | Interval | Extra charge/event | Events/day | mC/day |
|---|---|---|---|---|
| Baseline (1.1 mA × 86400 s) | — | — | — | 95,040 |
| LoRa TX (+0.7 mA × 10 s) | 3 min | 7 mC | 480 | 3,360 |
| SCD41 CO₂ | 15 min | 77 mC | 96 | 7,392 |
| SPS30 (55 mA × 30 s − idle) | 30 min | ~1,617 mC | 48 | 77,616 |
| **Total** | | | | **~183,408** |

`183,408 mC / 3600 = ~50.9 mAh/day` → **~24 days on a 1200 mAh LiPo with zero
solar input.**

> **The open problem, stated plainly.** The day-average baseline draw is
> **~1.1 mA**. The datasheet floor from the sleep currents of every component on
> the board is **~5.7 µA** — a factor of roughly 200. The suspects are SCD41 IR
> source leakage, BQ25185 quiescent current, and PCB leakage paths. **This is not
> solved yet, and closing it is the highest-impact open task on the project.**
>
> It is published rather than hidden because that is what an open-source hardware
> project is for: someone reproducing this board deserves to know the real number
> before they order parts, and someone with STM32WL leakage experience can now
> actually help. The issue is open on GitHub.

**Design decisions that are proven and unaffected by the above:**

| Decision | Effect |
|---|---|
| LoRaWAN instead of Wi-Fi / cellular | km of range at a fraction of the energy per byte |
| Single-chip STM32WLE5 (MCU + radio) | Removes the power and board cost of a discrete transceiver |
| SF7 + adaptive data rate (ADR) | Minimises on-air time — the largest per-event energy cost |
| Buck-boost across 2.5–4.2 V | No LDO dropout losses; battery used to its last usable volt |
| Staggered pre-wakeup (§2.2) | MCU sleeps through slow-sensor settling instead of busy-waiting |
| 3-minute duty cycle, adjustable over the air | Time resolution tunable per deployment without re-flashing |

Solar harvesting is designed to keep the node online indefinitely; the
long-term autonomy verification through a full seasonal cycle is running now and
is not yet complete.

---

## 5. Firmware & Data Pipeline

> **[IMAGE 5]** Live web dashboard showing real sensor data + the iOS app
> side by side.

**Measurement cycle (3 min, adjustable over the air):**

```
STOP2 sleep ─► pre-wake SPS30 (T−16.5 s) ─► SCD41 stabilisation shot (T−12 s)
     ▲                                                    │
     │                                          SCD41 useful shot (T−6 s)
     │                                                    ▼
     └── radio off, sensors asleep ◄── LoRaWAN TX ◄── read all + pack payload
```

- **Deterministic**, not event-driven: predictable energy per cycle, predictable
  battery life, predictable data density.
- **Compact binary payload codec** — every byte on air is energy spent, so the
  payload is bit-packed rather than JSON. The matching decoder is published for
  the network-server side.
- **LoRaWAN Class A** on the **Helium** network, EU868, built on the Semtech
  **LoRa Basics Modem**, with duty-cycle compliance handled in firmware.
- **Robust recovery paths**: incomplete SCD41 measurement sequences are rejected
  rather than transmitted as bad data; watchdog and progress recovery, battery
  recovery, and transactional NVM keep the node from bricking itself in the
  field. Persistence and modem anomalies no longer reset the station.
- **A real bug, found by a host-side test:** the uplink alarm overflowed
  reproducibly after **49.71 days** (2³² ms). Firmware 1.6 fixes it, and the
  regression test that caught it ships in the repository. Any node built from
  this design before that fix would have silently stopped transmitting after
  seven weeks.

**Backend:** Helium Console → **Amazon SNS** → HydroNode backend → public
dashboard + iOS app. **Automated anomaly detection** runs on the incoming stream
and flags implausible readings. Users can register their own stations, name them,
and browse historical charts per channel.

---

## 6. Enclosure

> **[IMAGE 6]** 3D-printed Stevenson screen, close-up of the stacked louvers.
>
> **[IMAGE 7]** CAD render of the Stevenson screen + PCB enclosure.

A sealed printed enclosure for the electronics, plus a **3D-printed Stevenson
screen** for the sensor head, printed in **white UV-resistant ASA**.

This is not decoration. **Stacked louvers block direct sun and driving rain while
letting air circulate freely.** Without it, the housing heats up in sunlight and
your temperature and humidity readings are simply wrong. ASA was chosen over PLA
and PETG specifically for outdoor UV stability — a PLA enclosure becomes brittle
within one season.

Mounting: standard pole or balcony railing. A waterproof cable gland handles the
solar lead.

**Both models (`.3mf`) are attached to this project and are printable on any
consumer FDM printer.**

> **Open detail — the UV window problem.** The LTR390 needs protection while
> still *seeing* UV. Ordinary glass and PETG block much of the UV band and
> falsify the reading. Proper UV-transparent quartz or borosilicate runs ~€50
> including shipping. Cheaper UV-A/UV-B-transparent films and optical materials
> are currently under test. **If you reproduce this, budget for the window
> material** — this is the one part where the cheap option quietly gives you
> wrong data.

---

## 7. Engineering Log — Including What Went Wrong

Open-source hardware is only useful if the failures are published too. Otherwise
everyone repeats them.

**Two faults were found on the first fabricated board. Both are fixed in the
published design.**

**1 — TPS63900 regulator section.** A wiring bug in the buck-boost section. Root
cause was identified and corrected in collaboration with an experienced
power-electronics engineer, and the corrected design drops into the same board
outline. In the meantime the deployed station runs on a deliberate workaround:
**the same TPS63900 on a breakout board plugged into the PCB headers** —
identical efficiency, minimal overhead, no compromise on the measurements. That
is why live data has been flowing throughout.

**2 — BQ25185 charger `CE` pin.** The charger has an internal 6-hour safety
timeout. If `CE` is not driven by an MCU GPIO, the firmware cannot reset that
timeout and charging silently stops after six hours — on a solar node that is
fatal and would have taken weeks of confusing field data to diagnose. The
corrected design routes `CE` to a GPIO that the firmware pulses HIGH for ~200 ms
each cycle. On boards already fabricated, the pin has to be hand-soldered to the
GPIO.

**3 — The 1.1 mA baseline (open).** See §4.3. Not solved, published anyway.

This is exactly the kind of iteration anyone reproducing the board should expect.
It is documented so that **you do not repeat it** — the published files contain
the fixed revision.

---

## 8. Reproduce It Yourself

Everything needed is attached to this project. No desktop EDA install required —
EasyEDA runs in the browser.

1. **Fork this project** in EasyEDA Pro (button at the top of this page).
2. **Order the PCB** directly from the editor via JLCPCB. 4 layers, standard
   process, no exotic requirements.
3. **Order the parts** — the BOM below links straight to LCSC part numbers.
   *Populate only the sensors you need* (see cost table).
4. **Assemble.** JLCPCB SMT handles the fine-pitch parts; the rest is basic
   hand-soldering.
5. **Print the enclosure** — both `.3mf` files attached. **ASA**, not PLA.
6. **Flash the firmware**, set your LoRaWAN `DevEUI` / `JoinEUI` / `AppKey`.
7. **Join a LoRaWAN network** (Helium, TTN, or your own gateway) and point the
   integration at your backend — or at the open HydroNode backend.

### Cost — scales with what you actually need

| Build | Sensors populated | Approx. parts cost |
|---|---|---|
| **Minimal** | Temperature + humidity + pressure (SHT30 substitute) | **< $30** |
| **Air quality** | + LTR390 UV, + SCD41 CO₂ | **~$70** |
| **Full research node** | All six channels incl. SPS30 particulate | **$120–150** |

The board is designed so **unpopulated sensor positions cost nothing and break
nothing.** One design serves a single hobbyist node and a research-grade
air-quality array.

---

## 9. Environmental Sustainability

Sustainability here is a design constraint, not a marketing line — it is the
reason the architecture looks the way it does.

- **Zero grid energy in operation.** Solar-harvested, battery-buffered, net energy
  consumption from the grid over the device lifetime: none.
- **Energy budget as a first-class constraint.** Every architectural choice —
  single-chip radio, buck-boost, staggered pre-wakeup, sub-second on-air time —
  exists to keep the harvesting panel small. Less silicon, less material, less
  embodied carbon than a node that brute-forces a bigger panel to cover a lazy
  power budget. The remaining 1.1 mA baseline (§4.3) is the last piece of that
  argument still to be earned, and it is stated openly rather than rounded away.
- **LoRaWAN over cellular.** No SIM, no carrier infrastructure load, orders of
  magnitude less energy per transmitted byte.
- **Long service life by material choice.** UV-resistant ASA instead of PLA, so
  the enclosure survives years outdoors instead of one season — the printed parts
  are not consumables.
- **Repairable and modular.** Through-hole headers for the sensor modules, test
  points on every rail, no glue, no potting, no proprietary connectors. A failed
  sensor is a $10 replacement, not a landfilled board.
- **Partial population = no waste.** You buy and mount only the sensors your use
  case needs.
- **The purpose is environmental.** Beyond its own footprint, the node exists to
  generate the air-quality and climate data that local environmental decisions
  currently lack.

---

## 10. Open Source

**All design files are published. Everything below is genuinely reusable.**

| Component | Licence |
|---|---|
| **Hardware** — schematic, PCB, CAD enclosure | **[CERN-OHL-S v2](https://cern-ohl.web.cern.ch/)** (strongly reciprocal) |
| **Documentation & media** | **[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)** |
| **Firmware** (own code) | **[MIT](https://opensource.org/license/mit)** |

*STMicroelectronics HAL / STM32CubeWL components retain their original ST
licence terms.*

**Why these licences:** CERN-OHL-S is the OSHWA- and OSI-aligned standard for
open hardware — anyone may build, sell, and modify the board, but improvements to
the hardware must be shared back under the same terms. The community gets the
design *and* keeps the derivatives. MIT on the firmware imposes no friction on
integration into other projects.

**Attached to this project:**

- ✅ Full **EasyEDA Pro** schematic + 4-layer PCB — forkable in one click
- ✅ **BOM** with LCSC part numbers, orderable directly
- ✅ **Gerber / pick-and-place** exports
- ✅ **`.3mf` Stevenson screen** — 3D-printable sensor head
- ✅ **`.3mf` PCB enclosure**
- ✅ **Firmware source** — build instructions, LoRaWAN payload codec, decoder
- ✅ **Payload decoder** for the network-server side
- ✅ This documentation

> 📦 **Firmware, tests, documentation and payload decoder — full repository:**
> **https://github.com/TexhFexLabs/hydroNodeStation01**
>
> Includes a `CONTRIBUTING.md` with the open problems, a `SECURITY.md`, and
> host-side regression tests you can run without any hardware.

---

## 11. Roadmap

Honest status of what is *not* finished:

- **The 1.1 mA baseline (§4.3)** — the highest-impact open task. Suspects
  identified, root cause not yet proven. Help genuinely welcome.
- **Rev B boards** with the corrected TPS63900 section and the routed BQ25185
  `CE` pin going into fabrication.
- **Solar autonomy long-term verification** — energy-balance logging through a
  full seasonal cycle is running now. No autonomy claim will be published before
  matched PPK2 measurements support it.
- **UV window material** — cheaper UV-transparent alternatives under test (§6).
- **CO₂ calibration** — guided FRC procedure and dynamic BMP390 pressure
  compensation for the SCD41. Automatic self-calibration is not supported in
  power-down single-shot mode, so this has to be explicit.
- **ML-based anomaly and calibration evaluation** on the accumulated dataset.
- **Multi-node deployment** — the actual point of the project: a dense local mesh,
  not a single station.

---

## 12. Thanks

This project would not exist in physical form without **EasyEDA**, **JLCPCB** and
**OSHWLab**. The complete 4-layer board — including the RF section — was designed
in **EasyEDA Pro**, ordered from **JLCPCB** straight out of the editor, and is
shared here on **OSHWLab** so that anyone can fork it and build their own.

Going from schematic to a working, deployed, data-streaming station in a browser,
without a single desktop EDA licence, is exactly what makes open hardware
accessible to students and hobbyists.

---

**Build it. Deploy it. Fork it. Join the network.**
**Live data is already flowing at [hydronode.texhfexlabs.de](https://hydronode.texhfexlabs.de).**
