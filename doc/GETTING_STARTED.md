# Firmware reference

The full path from ordering parts to live readings is in the
[build guide in the README](../README.md#build-a-station). This file is the
firmware-side reference: toolchain detail, flashing options, key handling and
debugging.

For timing, payload format and downlink commands see
[CONFIGURATION.md](CONFIGURATION.md). For fault recovery, flash migration and
the hardware tests still outstanding see [RELIABILITY.md](RELIABILITY.md).

---

## 1. Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| `arm-none-eabi-gcc` | 12 or newer | ARM GCC toolchain |
| `cmake` | 3.22 or newer | Build system |
| `ninja` or `make` | any | CMake generator backend |
| ST-Link V2 or J-Link | hardware | Flashing over SWD |
| `openocd` or STM32CubeProgrammer | any | Flash utility |
| USB to UART adapter, 3.3 V | hardware | Reading the debug console on PA6 |

```bash
# macOS
brew install arm-none-eabi-gcc cmake ninja

# Ubuntu / Debian
sudo apt install gcc-arm-none-eabi cmake ninja-build
```

---

## 2. Build

```bash
git clone https://github.com/TexhFexLabs/HydroNodeStation.git
cd HydroNodeStation/stm32_node
```

**Custom PCB, STM32WLE5CCU6:**

```bash
cmake -B build/Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Release
```

**Wio-E5 mini prototyping board:**

```bash
cmake -B build/Debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Debug
```

Output lands in the respective build directory as
`LoRaWAN_End_Node_LBM.elf`, `.bin` and `.hex`.

Both targets compile for STM32WLE5 and share all application logic. Only the
pin mapping and the MCU variant differ. GitHub Actions builds both on every
push, so a change that breaks one target is caught before it is merged.

Configure once. Afterwards `cmake --build build/Release` is enough, and
re-running the configure step with the toolchain flag is not necessary.

---

## 3. LoRaWAN keys

Create `stm32_node/LoRaWAN/App/se-identity-local.h`. The file is listed in
`.gitignore`, and `se-identity.h` includes it automatically when it exists.

```c
// Device EUI, leave at zero to derive it from the STM32 hardware UID
#define LORAWAN_DEVICE_EUI    00,00,00,00,00,00,00,00

// Join EUI, also called App EUI, big-endian, from your network server
#define LORAWAN_JOIN_EUI      AA,BB,CC,DD,EE,FF,00,11

// Application key, 16 bytes, big-endian, from your network server
#define LORAWAN_APP_KEY       AA,BB,CC,DD,EE,FF,00,11,22,33,44,55,66,77,88,99
#define LORAWAN_GEN_APP_KEY   AA,BB,CC,DD,EE,FF,00,11,22,33,44,55,66,77,88,99
```

Byte lists are comma-separated hexadecimal tokens without `0x`, matching the
format in the tracked default header.

`LORAWAN_GEN_APP_KEY` takes the same value as `LORAWAN_APP_KEY` under
LoRaWAN 1.0.x.

> **Never put real keys in the tracked `se-identity.h`, and never commit them.**
> Do not use `skip-worktree` or `.git/info/exclude` to hide production keys in a
> tracked file. The local header exists for exactly this purpose. See
> [SECURITY.md](../SECURITY.md).

A zero `LORAWAN_DEVICE_EUI` selects the silicon-derived ID, which is the
recommended setting for the custom PCB. A non-zero value is respected.

---

## 4. Flash

SWD pinout on the custom PCB header: `NRST`, `SWDCLK` on PA14, `SWDIO` on PA13,
`GND`. Supply the board from its own battery or externally at 3.3 V.

**OpenOCD with an ST-Link:**

```bash
openocd -f interface/stlink.cfg \
        -f target/stm32wlx.cfg \
        -c "program build/Release/LoRaWAN_End_Node_LBM.elf verify reset exit"
```

**STM32CubeProgrammer:** open the `.elf` or `.hex`, select ST-Link, click
Download.

> **Do not mass erase during an upgrade.** A normal firmware update has to
> preserve the last 8 KiB of flash, which holds the LoRaWAN context and the
> persisted configuration. See [RELIABILITY.md](RELIABILITY.md).

---

## 5. Read the Device EUI

After flashing, the LED blinks for 10 seconds. Connect the USB to UART adapter
to **PA6** at 115200 baud. The firmware prints the derived Device EUI on boot.

Register that EUI as an OTAA device on your network server using LoRaWAN 1.0.4
and the regional parameters for your region. Helium IoT, ChirpStack v4 and
The Things Network are all supported. The server returns the Join EUI and the
App Key for section 3.

The RF region is selected by `ACTIVE_REGION` in `LoRaWAN/App/lora_app.h`. The
compiled region set is in `LoRaWAN/Target/lorawan_conf.h`.

---

## 6. Connect to HydroNode

1. Open [hydronode.tech](https://hydronode.tech) or the HydroNode app and
   register. The first sensor slot is included at no cost.
2. Select the sensor, then open **Settings, LoRaWAN Binding**.
3. Enter the Device EUI and save.
4. HydroNode displays a webhook URL. Enter that URL on your network server under
   **Integrations, HTTP / Webhook**.

The backend decodes the payload by fPort and acknowledges uplinks
automatically. No decoder configuration is needed on your side.

To run your own backend instead, use [payload-decoder.js](payload-decoder.js).
Before deploying firmware 1.6 against an existing backend, make sure its
decoder treats the missing-value sentinels as absent readings rather than as
real measurements. The sentinels are documented in
[CONFIGURATION.md](CONFIGURATION.md).

### Additional sensor slots

Further sensors are available for a small cost contribution covering hardware
and running infrastructure. For academic or research use, write to
[contact@texhfexlabs.de](mailto:contact@texhfexlabs.de). Complimentary slots are
available for student and university projects.

---

## 7. Verify the first uplink

The first uplink goes out on fPort 2 and should reach the dashboard within one
to two minutes if there is gateway coverage.

If nothing arrives, check in this order:

1. Is the Device EUI on the network server identical to the one printed on PA6?
2. Did the join succeed? Enable logging (below) and watch the console.
3. Is there gateway coverage at the deployment site? Move the node closer to a
   known gateway to rule this out.
4. Is the webhook URL entered on the network server?

The join backoff is deliberate. Unsuccessful attempts run in windows of at most
five minutes, followed by pauses of 5, 10, 20, 40 and 60 minutes. The node does
not reboot merely because no gateway is in range.

---

## 8. Debugging

**Serial logging in normal operation.** Set `APP_LOG_ENABLED 1` in
`Core/Inc/sys_conf.h` and rebuild. The console appears on PA6 at 115200 baud.
Debug logs print the DevEUI and never the keys.

**Debug profile mode.** With `APP_LOG_ENABLED=1`, pull `DEBUG_SW_Pin` (PB4)
HIGH before power-on. The node skips LoRaWAN entirely and reads all sensors in a
loop, printing raw values on PA6 roughly every 8 seconds. This is the fastest
way to bring up sensors or check calibration without waiting for transmit
windows.

**Production defaults** are `APP_LOG_ENABLED=0` and `DEBUGGER_ENABLED=0`, with
`LOW_POWER_DISABLE=0` so STOP2 is active. Leaving logging on in the field costs
energy.
