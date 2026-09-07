# HydroNodeStation: Solar-Powered Open-Source Environmental Monitoring Node (STM32WLE5 + LoRaWAN)

> **[IMAGE 1, COVER]** Finished station mounted outdoors, Stevenson screen and
> solar panel visible, daylight. *Set this image as the project cover.*

A solar-powered, six-channel environmental sensor node built around a bare
STM32WLE5 die that carries the MCU and the LoRa radio on one chip. Designed
from scratch in EasyEDA Pro, fabricated at JLCPCB, deployed outdoors, and
streaming live data right now with no login required.

**Live public dashboard: https://hydronode.tech/stations/public/004dd7e7-5c27-4de0-bff5-24116f721658**

| | |
|---|---|
| **Status** | Built, assembled, deployed, field-tested. Live data flowing today |
| **Design tool** | 100 percent EasyEDA Pro, schematic and 4-layer PCB |
| **Fabrication** | JLCPCB, including SMT assembly |
| **Radio** | LoRaWAN Class A, EU868, Helium network, Semtech LoRa Basics Modem |
| **Measurement cycle** | 3 minutes, adjustable over the air from 30 to 3600 seconds |
| **Battery life, measured** | About 24 days on a 1200 mAh LiPo with zero solar input |
| **Board cost** | About 200 EUR for 5 assembled boards including shipping |
| **Sensor cost** | Under 30 USD minimal, 120 to 150 USD fully populated |
| **Licences** | Hardware CERN-OHL-S v2, docs CC BY-SA 4.0, firmware MIT |

> **Demo video:** `<YOUTUBE-LINK-HIER-EINFÜGEN>`
> Full build, deployment and live-data walkthrough.

---

## 1. The problem this solves

Air quality is a public health issue and climate variability is increasing, yet
hyperlocal environmental data is still scarce. Official monitoring stations are
expensive, need mains power, and are installed at only a handful of sites per
city. The gaps between them span kilometres. Whole neighbourhoods have no
measurements of the air they breathe.

The limiting factor has never been the sensors. It is power and connectivity. A
station that needs mains power or Wi-Fi can only go where that infrastructure
already exists, which is not where the data gaps are.

HydroNodeStation removes that constraint. It needs no mains power, no Wi-Fi and
no cellular contract. Put it on a pole, a rooftop, a balcony railing or a fence
in the middle of a field. It runs unattended and reports every three minutes
over a LoRaWAN link that spans kilometres.

The design goal was never a weather station. It was a station anyone can
reproduce, at a cost that scales with their budget, so that dense environmental
monitoring becomes something a community can build for itself.

---

## 2. What makes it different

Each point below was a deliberate engineering decision rather than a default.

### 2.1 Bare STM32WLE5 die, not a pre-made LoRa module

Almost every open-source LoRaWAN node uses a certified radio module: RAK, Seeed
E5, or an RFM95 next to a separate MCU. That is the easy path. It costs you
control of the RF front end, board area, and the module markup.

This design places the bare **STM32WLE5CCU6** on the board and builds the RF
chain itself. A **BALFHB-WL-02D3** integrated balun and a **BGS12SN6** TX/RX
switch on GPIO PC13 give properly matched, separate transmit and receive paths
at 868 MHz. The result is full control of impedance, ground return and antenna
feed, in a node that is smaller and cheaper than any module-based equivalent.

At the time of design, no suitable open-source STM32WLE5 reference design with
this sensor set existed. Closing that gap is the core open-source contribution
of this project.

### 2.2 Staggered pre-wakeup, so the MCU never waits on a sensor

The two interesting sensors are slow. The SCD41 needs about 5 seconds per
single-shot measurement. The SPS30 needs fan spin-up plus settling. A naive
implementation keeps the MCU awake through all of it, and that is where the
energy actually goes.

Instead the firmware schedules each slow sensor backwards from the transmit
window and sleeps through their measurement time.

- The **SCD41** runs two power-cycled single shots. A throwaway stabilisation
  shot starts 12 seconds before the uplink, then the useful shot starts
  6 seconds before it. Only the second is transmitted. The first exists purely
  to settle the photoacoustic cell after power-up. Without it, the first reading
  after a power cycle is measurably wrong.
- The **SPS30** starts 16.5 seconds before the uplink to cover fan spin-up and
  particle settling.

The MCU is in STOP2 for essentially all of that time.

### 2.3 Buck-boost instead of an LDO

Two **TPS63900** buck-boost converters supply the board. The primary rail holds
3.3 V across the entire battery curve from 2.5 V to 4.2 V and is always on. The
secondary rail produces 5 V for the SPS30 and is switched off entirely by a
GPIO when the particulate sensor is not measuring.

Two consequences matter. Sensor accuracy and radio output power stay constant
regardless of battery state, where an LDO-based node degrades slowly as the cell
drains. And the battery is used down to its last usable volt, because no energy
is burned as heat in a linear regulator.

### 2.4 Sensor-agnostic by architecture

All sensors sit on one I2C bus and are commanded into their own power-down
states between measurements. The reference sensor set is a starting point, not
a limit. Any I2C sensor can be added with a firmware adaptation: soil moisture,
VOC, noise, radiation, water level. The platform is an extensible research
instrument rather than a fixed product.

### 2.5 Remote control without a site visit

An unattended node is only maintainable if you can change its behaviour over
the air. The firmware accepts LoRaWAN downlink commands on fPort 2.

| Command | Effect |
|---|---|
| `10 HH LL` | Set the transmit interval in seconds, 30 to 3600, persisted to flash |
| `11` | Trigger an SPS30 fan-cleaning cycle |
| `12` | Request boot, reset and sensor diagnostics on fPort 5 |
| `FF` | Software reset |

Combined with the transactional NVM layer, a station can be re-tuned, diagnosed
and recovered without anyone climbing to it. The SPS30 additionally cleans
itself every 120 hours, but only when the battery is above 4120 mV, so
maintenance can never brown out the node.

---

## 3. What it measures

> **[IMAGE 2]** Assembled PCB, top side, sensors populated, macro shot.

| Quantity | Sensor | I2C address | Accuracy |
|---|---|---|---|
| CO2 concentration | Sensirion **SCD41** | 0x62 | Single-shot photoacoustic NDIR, plus or minus 40 ppm |
| Particulate matter | Sensirion **SPS30** | 0x69 | PM1.0, PM2.5, PM4, PM10 and number concentration |
| Temperature and humidity | Sensirion **SHT45** | 0x44 | plus or minus 0.1 degrees C, plus or minus 1.0 percent RH |
| Barometric pressure | Bosch **BMP390** | 0x77 | plus or minus 0.5 hPa absolute |
| UV index | Lite-On **LTR390** | 0x53 | UVA and ambient light, needs a UV-transmitting window |
| Battery state of charge | Maxim **MAX17048** | 0x36 | I2C fuel gauge, no sense resistor |

All six channels are live and validated in the field. This is measured
hardware, not a bill of materials wish list.

> **The sensor set changed during development, and the reasoning is published.**
> The original BME280 was replaced by SHT45 plus BMP390, which is more accurate
> and lower power at a similar cost. The analog VEML6075 was replaced by the
> LTR390, which has a wider range, a native I2C interface, and does not consume
> an ADC channel. An ambient noise channel was evaluated and dropped to keep the
> power budget honest. Anyone reproducing this board gets the second iteration
> of these decisions rather than the first guess.

---

## 4. Hardware design

> **[IMAGE 3]** EasyEDA Pro 3D render of the 4-layer PCB, top and bottom.

### 4.1 PCB

Four layers, designed from scratch in EasyEDA Pro.

**Stackup.** Top and bottom carry signal and supply traces. Inner 1 is a
dedicated ground layer with a continuous copper pour. Inner 2 is a second signal
layer. The continuous ground layer gives short return paths and keeps
common-mode noise off the I2C and control lines.

**RF section.** Traces are 50 ohm controlled impedance for 868 MHz. A dense
ground via fence surrounds the balun, the RF switch, the decoupling capacitors
and the SMA path. It ties the RF field down to the ground layer and suppresses
coupling into the rest of the board. Keep-out zones follow the manufacturer's
reference layout, which is the single most common omission in DIY LoRa boards.
The antenna is external on an SMA connector.

**Connectors.** A 2-pin JST connector with a parallel screw terminal for the
battery. A 2-pin screw terminal for the solar panel. A 6-pin screw terminal for
the cable running to the Stevenson screen, carrying I2C, supply and the panel
lead. A 4-pin header for the ST-Link V2: NRST, SWDCLK, SWDIO, GND.

**Test pads** are provided on the supply rails, the I2C bus and the charger CE
pin. A board you cannot probe is a board you cannot debug.

The RF layout and antenna matching were by far the most time-intensive part of
the project, and are where most of the reusable open-source value sits. Anyone
forking this board inherits an RF section that has been iterated and measured
rather than copied from a datasheet diagram.

### 4.2 Power path

> **[IMAGE 4]** Enclosure open, PCB installed, wiring and solar cable gland
> visible.

```
Solar panel ──► BQ25185 ──► 1S LiPo ──► TPS63900 #1 ──► 3.3 V, always on
6.91 V         (charge      1200 mAh   (buck-boost)     ├─► STM32WLE5 + RF
570 mW          controller) 2.5-4.2 V                   └─► I2C sensor bus
                                       TPS63900 #2 ──► 5 V, GPIO switched
                                       (buck-boost)    └─► SPS30
```

- **BQ25185** solar charge controller. Harvests from a small panel, handles the
  full LiPo charge profile, and keeps the system running while charging.
- **TPS63900**, two of them. Under 0.5 microamp quiescent, over 95 percent
  efficiency. See section 2.3.
- **MAX17048** fuel gauge. Real state-of-charge telemetry is sent with every
  packet, so an operator can see a node's energy budget remotely instead of
  guessing.

**Solar panel and battery as deployed.** An AnySolar SOLAR CELL G3 THIN, 6.91 V
open circuit, 570.8 mW, Vmpp 5.58 V, Impp 102 mA, feeding a 1200 mAh 1S LiPo.
That combination is fully sufficient through summer. Winter operation is being
tested now. Do not use a smaller panel: same size or larger only. The battery
can be scaled with the panel, but going much smaller is risky, because several
overcast days in a row will drain it.

### 4.3 Measured power budget, and one unsolved problem

These are PPK2 measurements on the actual custom PCB, not datasheet arithmetic.
They are published in full, including the part that does not look good yet.

| Source | Interval | Extra charge per event | Events per day | mC per day |
|---|---|---|---|---|
| Baseline, 1.1 mA over 86400 s | | | | 95,040 |
| LoRa transmit, +0.7 mA for 10 s | 3 min | 7 mC | 480 | 3,360 |
| SCD41 CO2 | 15 min | 77 mC | 96 | 7,392 |
| SPS30, 55 mA for 30 s minus idle | 30 min | 1,617 mC | 48 | 77,616 |
| **Total** | | | | **183,408** |

`183,408 mC / 3600 = 50.9 mAh per day`, which gives about **24 days on a
1200 mAh cell with zero solar input**.

> **The open problem, stated plainly.** The day-average baseline draw is
> 1.1 mA. Adding up the sleep currents of every component on the board gives
> roughly 5.7 microamps, a factor of about 200. The suspects are SCD41 IR source
> leakage, BQ25185 quiescent current, and PCB leakage paths. This is not solved,
> and closing it is the highest-impact open task on the project.
>
> It is published rather than hidden because that is what an open-source
> hardware project is for. Someone reproducing this board deserves to know the
> real number before ordering parts, and someone with STM32WL leakage experience
> can now actually help. The issue is open on GitHub.

Design decisions that are proven and unaffected by the above:

| Decision | Effect |
|---|---|
| LoRaWAN instead of Wi-Fi or cellular | Kilometres of range at a fraction of the energy per byte |
| Single-chip STM32WLE5 | Removes the power and board cost of a discrete transceiver |
| SF7 with adaptive data rate | Minimises on-air time, the largest per-event energy cost |
| Buck-boost across 2.5 to 4.2 V | No LDO dropout losses, battery used to its last usable volt |
| Staggered pre-wakeup, section 2.2 | MCU sleeps through slow-sensor settling |
| 5 V rail switched off by GPIO | The SPS30 draws nothing between measurements |
| 3 minute cycle, adjustable over the air | Time resolution tunable per deployment without reflashing |

Solar harvesting is designed to keep the node online indefinitely. Energy
balance logging through a full seasonal cycle is running now and is not
complete.

---

## 5. Firmware and data pipeline

> **[IMAGE 5]** Live web dashboard and the iOS app side by side.

Measurement cycle, 3 minutes, adjustable over the air:

```
STOP2 sleep ──► pre-wake SPS30 (T-16.5 s) ──► SCD41 stabilisation shot (T-12 s)
     ▲                                                    │
     │                                       SCD41 useful shot (T-6 s)
     │                                                    ▼
     └── radio off, sensors asleep ◄── LoRaWAN TX ◄── read all, pack payload
```

- **Deterministic rather than event-driven.** Predictable energy per cycle,
  predictable battery life, predictable data density.
- **Compact binary payload codec.** Every byte on air costs energy, so the
  payload is bit-packed rather than JSON. The matching decoder is published for
  the network-server side.
- **CO2 every fifth uplink, particulate matter every tenth.** The expensive
  sensors do not run every cycle.
- **LoRaWAN Class A** on the Helium network, EU868, built on the Semtech LoRa
  Basics Modem, with duty-cycle compliance handled in firmware.
- **Robust recovery paths.** Incomplete SCD41 measurement sequences are rejected
  rather than transmitted as bad data. Watchdog and progress recovery, battery
  recovery, and two CRC-protected flash snapshots keep the node from bricking
  itself in the field.
- **Battery policy with hysteresis.** Below 3500 mV the interval doubles. Below
  3300 mV the node cancels radio and sensor work and checks the voltage every
  60 seconds until it recovers.

**A real bug, found by a host-side test.** The uplink alarm overflowed
reproducibly after 49.71 days, which is 2^32 milliseconds. Firmware 1.6 fixes
it, and the regression test that caught it ships in the repository. Any node
built from this design before that fix would have silently stopped transmitting
after seven weeks.

**Backend.** The network server posts every uplink straight to the HydroNode
backend over an HTTPS webhook, with no message broker in between. That keeps
the whole pipeline reproducible by anyone running their own server.

```
Network server ──HTTPS webhook──► HydroNode API ──► Kafka pipeline ──► PostgreSQL
Helium, ChirpStack v4 or TTN                        validation             │
                                                                           ▼
                                                   public dashboard + iOS app
```

Automated anomaly detection runs on the incoming stream and flags implausible
readings. Users can register their own stations, name them, and browse
historical charts per channel. Because the decoder is published and the
integration is a plain webhook, you can point a station at your own backend
instead. Nothing here is locked to my infrastructure.

---

## 6. Enclosure

> **[IMAGE 6]** 3D-printed Stevenson screen, close-up of the stacked louvers.
>
> **[IMAGE 7]** CAD render of the Stevenson screen and the PCB housing.

A sealed printed housing for the electronics, plus a 3D-printed Stevenson screen
for the sensor head, designed in Fusion 360 and printed in white UV-resistant
ASA.

This is not decoration. Stacked louvers block direct sun and driving rain while
air circulates freely. Without it the housing heats up in sunlight and the
temperature and humidity readings are simply wrong. ASA was chosen over PLA and
PETG for outdoor UV stability, because a PLA enclosure becomes brittle within a
season. Several print runs were needed to get the fit right.

Mounting is on a standard pole or balcony railing. A waterproof cable gland
handles the solar lead.

Both models are attached to this project as `.3mf` and print on any consumer
FDM printer.

> **Open detail, the UV window.** The LTR390 has to see UV through the
> enclosure, but ordinary glass and PETG block most of the UV band and produce a
> reading that is wrong rather than missing. The deployed station uses roughly
> 2 mm of polystyrene with an empirical correction factor of 17/13, applied in
> the firmware as integer arithmetic. That was a cost decision. It works, but
> polystyrene is not UV-stable and will degrade outdoors. UV-transmitting
> acrylic (PMMA UVT) is the better choice for a station meant to last. Quartz or
> borosilicate is better still and costs around 50 EUR for a small disc. If you
> reproduce this, budget for the window material. It is the one part where the
> cheap option quietly gives you wrong data.

---

## 7. Engineering log, including what went wrong

Open-source hardware is only useful if the failures are published too.
Otherwise everyone repeats them.

Two faults were found on the first fabricated board. Both are fixed in the
published design.

**1. TPS63900 regulator section.** A wiring bug in the buck-boost section. The
root cause was identified and corrected together with an experienced power
electronics engineer, and the corrected design drops into the same board
outline. In the meantime the deployed station runs on a deliberate workaround:
the same TPS63900 on a breakout board plugged into the PCB headers. Efficiency
is identical and the measurements are unaffected, which is why live data has
been flowing throughout.

**2. BQ25185 charger CE pin.** The charger has an internal 6 hour safety
timeout. If CE is not driven by an MCU GPIO, the firmware cannot reset that
timeout and charging stops silently after six hours. On a solar node that is
fatal, and it would have taken weeks of confusing field data to diagnose. The
corrected design routes CE to a GPIO that the firmware pulses. On boards already
fabricated, the pin has to be hand-soldered to the GPIO.

**3. The 1.1 mA baseline, still open.** See section 4.3. Not solved, published
anyway.

A further honest note for anyone deploying outdoors: in the published schematic
the BQ25185 TS/MR pin uses a fixed 10 kilohm resistor, so the charger does not
measure battery temperature. Winter operation needs an appropriate battery NTC
and a verified undervoltage path. The firmware cannot protect an unprotected
cell below MCU operating voltage.

---

## 8. Reproduce it yourself

Everything needed is attached to this project. No desktop EDA installation is
required, because EasyEDA runs in the browser.

1. **Fork this project** in EasyEDA Pro using the button at the top of this
   page.
2. **Order the PCB** directly from the editor through JLCPCB. Four layers,
   standard process, no exotic requirements. About 200 EUR for 5 assembled
   boards including shipping, 2 to 3 weeks lead time.
3. **Enable SMT assembly.** The STM32WLE5, the balun and the RF switch are
   fine-pitch parts, and hand-soldering them is where most reproduction attempts
   fail.
4. **Order the parts.** The bill of materials on this page carries LCSC part
   numbers and can be ordered directly. Populate only the sensors you need.
5. **Source the rest.** Solar panel, 1S LiPo, 868 MHz SMA antenna, 6-core cable,
   waterproof cable gland, UV window material, ST-Link V2, USB to UART adapter.
6. **Print the enclosure.** Both `.3mf` files are attached. White ASA, not PLA.
7. **Build and flash the firmware.** The Device EUI is derived automatically
   from the STM32 unique ID and printed over the debug UART on boot. You only
   register that EUI with your network server and set your own Join EUI and App
   Key.
8. **Join a LoRaWAN network** (Helium, ChirpStack or TTN) and point its HTTP
   webhook at your backend, or at the open HydroNode backend.

The full step-by-step guide, including toolchain setup, flashing and the
HydroNode binding, is in the GitHub repository README.

### Cost, scaled to what you need

| Build | Sensors populated | Sensor cost |
|---|---|---|
| **Minimal** | Temperature, humidity, pressure with an SHT30 substitute | under 30 USD |
| **Air quality** | Plus LTR390 UV and SCD41 CO2 | around 70 USD |
| **Full research node** | All six channels including SPS30 particulate | 120 to 150 USD |

Sensors only. The board itself, the RF section, the power stage and the
fabrication come on top, roughly 200 EUR for 5 assembled boards.

Unpopulated sensor positions cost nothing and break nothing. One design serves a
single hobbyist node and a research-grade air quality array.

---

## 9. Environmental sustainability

Sustainability here is a design constraint rather than a marketing line. It is
the reason the architecture looks the way it does.

- **Zero grid energy in operation.** Solar harvested, battery buffered. Net grid
  consumption over the device lifetime is none.
- **Energy budget as a first-class constraint.** Every architectural choice,
  from the single-chip radio to the buck-boost converters, the staggered
  pre-wakeup and the switched 5 V rail, exists to keep the harvesting panel
  small. Less silicon, less material, less embodied carbon than a node that
  compensates for a lazy power budget with a bigger panel. The remaining 1.1 mA
  baseline in section 4.3 is the last piece of that argument still to be earned,
  and it is stated openly rather than rounded away.
- **LoRaWAN over cellular.** No SIM, no load on carrier infrastructure, orders
  of magnitude less energy per transmitted byte.
- **Long service life by material choice.** UV-resistant ASA instead of PLA, so
  the enclosure survives years outdoors instead of one season. The printed parts
  are not consumables.
- **Repairable and modular.** Headers for the sensor modules, test points on
  every rail, no glue, no potting, no proprietary connectors. A failed sensor is
  a 10 USD replacement, not a landfilled board.
- **Partial population means no waste.** You buy and mount only the sensors your
  use case needs.
- **The purpose is environmental.** Beyond its own footprint, the node exists to
  generate the air quality and climate data that local environmental decisions
  currently lack.

---

## 10. Open source

All design files are published, and everything below is genuinely reusable.

| Component | Licence |
|---|---|
| Hardware: schematic, PCB, CAD enclosure | [CERN-OHL-S v2](https://cern-ohl.web.cern.ch/), strongly reciprocal |
| Documentation and media | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| Firmware, own code | [MIT](https://opensource.org/license/mit) |

Vendored third-party components keep their upstream terms: STMicroelectronics
HAL and utilities under BSD-3-Clause, Arm CMSIS under Apache-2.0, and the
Semtech LoRa Basics Modem under BSD-3-Clause.

**Why these licences.** CERN-OHL-S is the OSHWA- and OSI-aligned standard for
open hardware. Anyone may build, sell and modify the board, but improvements to
the hardware have to be shared back under the same terms, so the community keeps
the derivatives. MIT on the firmware imposes no friction on reuse in unrelated
projects.

**Attached to this project:**

- Full EasyEDA Pro schematic and 4-layer PCB, forkable in one click, with
  Gerbers and pick-and-place generated straight from the editor
- Bill of materials with LCSC part numbers, orderable directly
- `.3mf` Stevenson screen for the sensor head
- `.3mf` housing for the PCB
- This documentation

**In the GitHub repository:**

- Full firmware source with a CMake build for both board targets
- Payload decoder for the network-server side
- Host-side regression tests that run without any hardware
- A complete build guide, plus configuration and reliability references

**Repository: https://github.com/TexhFexLabs/HydroNodeStation**

---

## 11. Roadmap

Honest status of what is not finished.

- **The 1.1 mA baseline**, section 4.3. The highest-impact open task. Suspects
  identified, root cause not yet proven. Help is genuinely welcome.
- **Revision 2 boards** with the corrected TPS63900 section and the routed
  BQ25185 CE pin, going into fabrication.
- **Solar autonomy verification.** Energy balance logging through a full
  seasonal cycle is running now. No autonomy claim will be published before
  matched PPK2 measurements support it.
- **UV window material.** Cheaper UV-transmitting alternatives to polystyrene
  are under test, see section 6.
- **CO2 calibration.** A guided forced-recalibration procedure and dynamic
  BMP390 pressure compensation for the SCD41. Automatic self-calibration is not
  supported in power-down single-shot mode, so this has to be explicit.
- **Machine learning evaluation** for anomaly detection and calibration on the
  accumulated dataset.
- **Multi-node deployment.** The actual point of the project is a dense local
  mesh, not a single station.

---

## 12. Thanks

This project would not exist in physical form without EasyEDA, JLCPCB and
OSHWLab. The complete 4-layer board including the RF section was designed in
EasyEDA Pro, ordered from JLCPCB straight out of the editor, and shared here on
OSHWLab so that anyone can fork it and build their own.

Going from schematic to a working, deployed, data-streaming station in a
browser, without a single desktop EDA licence, is what makes open hardware
accessible to students and hobbyists.

---

**Build it. Deploy it. Fork it.**
Live data is flowing at https://hydronode.tech
