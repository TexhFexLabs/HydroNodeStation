# Configuration Reference

All the knobs you might need to turn when adapting HydroNodeStation01 to your setup.

---

## LoRaWAN Keys

**File:** `stm32_node/LoRaWAN/App/se-identity.h`

| Constant | Description |
|---|---|
| `LORAWAN_DEVICE_EUI` | Device EUI (big-endian). Set to all zeros to auto-read from STM32 silicon UID. |
| `LORAWAN_JOIN_EUI` | Join EUI / App EUI (big-endian). |
| `LORAWAN_APP_KEY` | 16-byte application key (big-endian). |
| `LORAWAN_GEN_APP_KEY` | Set to same value as `LORAWAN_APP_KEY` for LoRaWAN 1.0.x networks. |

Format: bytes separated by commas, no `0x` prefix — e.g. `AA,BB,CC,DD,EE,FF,00,11`.

---

## Build-Time Feature Flags

**File:** `stm32_node/Core/Inc/sys_conf.h`

| Flag | Default | Effect |
|---|---|---|
| `LOW_POWER_DISABLE` | `0` | Set to `1` to disable STOP2 sleep — useful for debugging with a debugger attached (STOP2 drops the debug connection). |
| `APP_LOG_ENABLED` | `0` | Set to `1` to enable SW-UART log output on PA6 in normal LoRaWAN mode. |
| `SCD41_ENABLED` | `1` | Set to `0` to skip CO₂ measurements entirely (saves ~43% of power budget). |

---

## TX Duty Cycle & Timing

**File:** `stm32_node/LoRaWAN/App/lora_app.h`

| Constant | Default | Effect |
|---|---|---|
| `APP_TX_DUTYCYCLE` | `300000` ms (5 min) | Time between uplinks in normal operation. |
| `LOW_BAT_TX_DUTYCYCLE` | `600000` ms (10 min) | Duty cycle when battery < 3400 mV. |
| `CRITICAL_BAT_TX_DUTYCYCLE` | `3600000` ms (60 min) | Duty cycle when battery < 3000 mV (base sensors only). |
| `SCD41_PREMEASUREMENT_MS` | `5500` | Wake-up lead time for SCD41 before TX window. Do not reduce below 5000. |
| `SPS30_PREMEASUREMENT_MS` | `16500` | Wake-up lead time for SPS30 before TX window. Do not reduce below 16000. |

Battery voltage thresholds (also in `lora_app.h`):

| Constant | Default | Meaning |
|---|---|---|
| `LOW_BATTERY_THRESHOLD` | `3400` mV | Below this: reduced duty cycle, all sensors still active. |
| `CRITICAL_BATTERY_THRESHOLD` | `3000` mV | Below this: max duty cycle, only SHT45 + BMP390 + battery sent. |

---

## SW-UART (Debug Log Output)

**File:** `stm32_node/Core/Inc/sw_uart.h`

| Constant | Default | Effect |
|---|---|---|
| `SW_UART_BAUDRATE` | `115200` | Baud rate of the bit-banged UART on PA6. |

Connect a USB-UART adapter to **PA6** to receive log output. UART is TX-only (no RX). Enable logging in normal mode by setting `APP_LOG_ENABLED 1` in `sys_conf.h`.

---

## Sensor Enable / Disable

Individual sensors can be excluded at compile time or by depopulating the component on the PCB. In firmware, each sensor is guarded by its own I2C address check during `EnvSensors_Init()` — if a sensor doesn't ACK, it is flagged as absent and skipped silently.

To permanently exclude a sensor in firmware, comment out the corresponding init and read calls in `stm32_node/Core/Src/sys_sensors.c`.

---

## TX Payload Format

All fields are **2-byte big-endian unsigned integers**.

### fPort 2 — Base sensors (every TX)

| Byte offset | Field | Scale | Unit |
|---|---|---|---|
| 0–1 | Temperature | ÷ 100 | °C |
| 2–3 | Humidity | ÷ 100 | %RH |
| 4–5 | Pressure | ÷ 10 | hPa |
| 6–7 | Battery voltage | raw | mV |
| 8–9 | UV index × 100 | ÷ 100 | UVI |

### fPort 3 — Base + CO₂ (every 3rd TX)

fPort 2 fields, followed by:

| Byte offset | Field | Scale | Unit |
|---|---|---|---|
| 10–11 | CO₂ concentration | raw | ppm |

### fPort 4 — Base + CO₂ + Particulates (every 6th TX)

fPort 3 fields, followed by:

| Byte offset | Field | Scale | Unit |
|---|---|---|---|
| 12–13 | PM1.0 mass | ÷ 10 | µg/m³ |
| 14–15 | PM2.5 mass | ÷ 10 | µg/m³ |
| 16–17 | PM4.0 mass | ÷ 10 | µg/m³ |
| 18–19 | PM10 mass | ÷ 10 | µg/m³ |
| 20–21 | NC0.5 | ÷ 10 | #/cm³ |
| 22–23 | NC1.0 | ÷ 10 | #/cm³ |
| 24–25 | NC2.5 | ÷ 10 | #/cm³ |
| 26–27 | NC4.0 | ÷ 10 | #/cm³ |
| 28–29 | NC10 | ÷ 10 | #/cm³ |
| 30–31 | Typical particle size | ÷ 1000 | µm |

### Payload Decoder

Paste this into your network server's JavaScript decoder (Helium, TTN, ChirpStack all support this format):

```javascript
function decodeUplink(input) {
  const bytes = input.bytes;
  const port = input.fPort;
  let data = {};

  function u16(i) {
    return (bytes[i] << 8) | bytes[i + 1];
  }

  data.temperature  = u16(0) / 100;
  data.humidity     = u16(2) / 100;
  data.pressure     = u16(4) / 10;
  data.battery_mv   = u16(6);
  data.uvi          = u16(8) / 100;

  if (port >= 3) {
    data.co2_ppm = u16(10);
  }

  if (port >= 4) {
    data.pm1_0   = u16(12) / 10;
    data.pm2_5   = u16(14) / 10;
    data.pm4_0   = u16(16) / 10;
    data.pm10    = u16(18) / 10;
    data.nc0_5        = u16(20) / 10;
    data.nc1_0        = u16(22) / 10;
    data.nc2_5        = u16(24) / 10;
    data.nc4_0        = u16(26) / 10;
    data.nc10         = u16(28) / 10;
    data.typ_size_um  = u16(30) / 1000;
  }

  return { data };
}
```

---

## RF Region

**File:** `stm32_node/LoRaWAN/Target/lorawan_conf.h`

Change `ACTIVE_REGION` to match your region:

```c
#define ACTIVE_REGION LORAMAC_REGION_EU868   // default
// LORAMAC_REGION_US915
// LORAMAC_REGION_AU915
// LORAMAC_REGION_AS923
// etc.
```

Make sure the Semtech LBM middleware is also built with the matching region flag — this is set in `cmake/stm32cubemx/CMakeLists.txt` under the LBM compile definitions.
