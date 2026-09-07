# Getting Started

This guide walks you through building the firmware, connecting to a LoRaWAN network, and linking the device to the HydroNode platform.

---

## 1. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| `arm-none-eabi-gcc` | ≥ 12 | ARM GCC toolchain |
| `cmake` | ≥ 3.22 | Build system |
| `ninja` or `make` | any | CMake generator backend |
| ST-Link or J-Link | any | For flashing via SWD |
| `openocd` or STM32CubeProgrammer | any | Flash utility |

**macOS:**
```bash
brew install arm-none-eabi-gcc cmake ninja
```

**Ubuntu/Debian:**
```bash
sudo apt install gcc-arm-none-eabi cmake ninja-build
```

---

## 2. Clone & Build

```bash
git clone https://github.com/TexhFexLabs/HydroNodeStation.git
cd hydroNodeStation/stm32_node

# Configure (once)
cmake -B build/Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake

# Build
cmake --build build/Release
```

Output: `build/Release/LoRaWAN_End_Node_LBM.elf` (and `.bin`, `.hex`).

---

## 3. Configure LoRaWAN Keys

Create the Git-ignored `stm32_node/LoRaWAN/App/se-identity-local.h` and fill in your credentials from your LoRaWAN network server:

```c
// Device EUI — unique per device (big-endian, byte-by-byte)
#define LORAWAN_DEVICE_EUI    AA,BB,CC,DD,EE,FF,00,11

// Join EUI / App EUI (big-endian)
#define LORAWAN_JOIN_EUI      AA,BB,CC,DD,EE,FF,00,11

// Application Key — 16 bytes (big-endian)
#define LORAWAN_APP_KEY       AA,BB,CC,DD,EE,FF,00,11,22,33,44,55,66,77,88,99
#define LORAWAN_GEN_APP_KEY   AA,BB,CC,DD,EE,FF,00,11,22,33,44,55,66,77,88,99
```

> **Do not commit real keys.** The local header is ignored automatically. Keep the tracked default header unchanged.

If `LORAWAN_DEVICE_EUI` is set to all zeros, the firmware automatically reads the unique hardware ID burned into the STM32WLE5 silicon — recommended for the custom PCB.

---

## 4. Flash the Firmware

**With OpenOCD (ST-Link):**
```bash
openocd -f interface/stlink.cfg \
        -f target/stm32wlx.cfg \
        -c "program build/Release/LoRaWAN_End_Node_LBM.elf verify reset exit"
```

**With STM32CubeProgrammer (GUI):** open the `.elf` or `.hex`, select ST-Link, click Download.

SWD pinout: `SWDIO` → PA13, `SWDCLK` → PA14, `GND` → GND, `3.3V` → VDD (or power externally).

---

## 5. Register on a LoRaWAN Network Server

Create an OTAA device with the same Device EUI / Join EUI / App Key as in `se-identity.h`:

- **LoRaWAN version:** 1.0.4
- **Regional parameters:** EU868 (or your region)

Supported network servers: **Helium IoT**, **ChirpStack**, **The Things Network (TTN)**. The steps below use Helium IoT as the reference; other servers are similar.

---

## 6. Connect to HydroNode via HTTP webhook

Use the HTTP webhook URL supplied by the station's LoRaWAN binding settings in HydroNode. Configure that URL on the network server. Before deploying firmware 1.6, ensure its decoder treats the new missing-value sentinels as absent readings; see [payload-decoder.js](payload-decoder.js) and [reliability notes](RELIABILITY.md).

---

## 7. Link the Device in HydroNode

1. Open the HydroNode app or web dashboard at [hydronode.tech](https://hydronode.tech).
2. Your first sensor slot is included automatically upon registration at no charge.
3. Go to the sensor's **Settings → LoRaWAN Binding**.
4. Enter your device's **DevEUI**.

Once saved, sensor readings appear automatically as uplinks arrive — no decoder configuration needed, the HydroNode backend handles the payload format.

### Sensor Pricing

Additional sensors are available for a small cost contribution that covers hardware and ongoing infrastructure costs. If you are using HydroNode for academic or research purposes, please reach out to [contact@texhfexlabs.de](mailto:contact@texhfexlabs.de) — complimentary sensor slots are available for student and university projects.

---

## 8. Verify First Uplink

After flashing, the node blinks its LED for 10 seconds, then starts the LoRaWAN join. The first uplink (fPort 2) should appear in the HydroNode dashboard within 1–2 minutes if there is Helium coverage.

**To see raw sensor output without LoRaWAN**, use Debug Profile Mode below.

### Debug Profile Mode

Set `APP_LOG_ENABLED=1`, then pull `DEBUG_SW_Pin` (PB4) **HIGH** before power-on. The node skips LoRaWAN and reads all sensors in a loop, printing values over SW-UART.

Connect a USB-UART adapter to **PA6** at 115200 baud. Readings appear every few seconds.

To enable SW-UART logs in normal LoRaWAN mode, set `APP_LOG_ENABLED 1` in `Core/Inc/sys_conf.h` and rebuild.

---

## 9. Debug build and hardware validation

The current Debug and Release CMake targets both compile for STM32WLE5. Legacy prototype pin mappings are not a separate supported target in these presets. See [RELIABILITY.md](RELIABILITY.md) for flash migration, watchdog debugging and required hardware tests. A normal upgrade must preserve the last 8 KiB of flash; avoid mass erase.
