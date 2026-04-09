# Stromverbrauch-Analyse HydroNodeStation01

**Datum:** 2026-04-05  
**Firmware-Stand:** branch `21-update-ioc-to-match`  
**Systemspannung:** 3.3 V

---

## 1. Komponentenspezifikationen (Quellen)

### STM32WLE5JC
| Zustand | Strom | Quelle |
|---|---|---|
| STOP2 + RTC aktiv | **1.07 µA** | DS13105 Rev12, Titelseite, VDD = 3 V |
| Aktiv MCU (48 MHz, kein Radio) | **~5 mA** | < 72 µA/MHz × 48 MHz = 3.456 mA + Peripherie-Overhead |
| LoRa RX | **4.82 mA** | DS13105 Rev12, Titelseite |
| LoRa TX @ 14 dBm (LP PA) | **~22 mA** | DS13105, interpoliert: 15 mA @ 10 dBm, LP PA max ~15 dBm |

> Alle TX/RX-Ströme sind Gesamt-Chip-Ströme (MCU + Radio kombiniert).

### SCD41 (Sensirion)
| Zustand | Strom / Ladung | Quelle |
|---|---|---|
| power_down-Modus (Sleep) | **~0 µA** | SCD4x Low Power App Note, Kap. 2.4: eliminiert Idle-Strom |
| RHT single shot (wake_up 30 ms + Messung 50 ms) | **~2 mA** über **80 ms** = **0.16 mC** | Schätzung (kein CO₂-IR-Emitter aktiv) |
| CO₂ single shot (power-cycled, 1 Shot steady-state) | **q = 77 mC** @ 3.3 V | SCD4x Low Power App Note, Kap. 3.3: q_ss = 77 mC / shot |

> **Hinweis q_pcss vs. q_ss:**  
> Das Datenblatt nennt q_pcss = 154 mC pro *nützlichem* Shot, wobei es annimmt, dass nach jedem  
> Power-Cycle **2 Shots** gemacht werden (1 Verworfen + 1 Nützlich). Die Firmware verwirft nur beim  
> **ersten Start nach dem Boot** (Flag `TxDiscardFirstCo2`). Im Steady-State: **1 Shot = 77 mC**.  
> Konservative Schätzung mit 154 mC → Akkulaufzeit halbiert sich beim SCD41-Anteil.

### SHT45 (Sensirion SHT4x)
| Zustand | Strom | Quelle |
|---|---|---|
| Deep Sleep (Standby) | **0.1 µA** | Sensirion SHT4x Datasheet, Sleep Mode |
| Single Shot High Repeatability | 2.0 mA × 8.3 ms = **0.0166 mC** | SHT4x Datasheet |

> **Im Firmware nicht implementiert** — bleibt die gesamte Zeit im Deep Sleep.

### BMP390 (Bosch, als „BME390" angenommen)
| Zustand | Strom | Quelle |
|---|---|---|
| Sleep Mode | **2.0 µA** | Bosch BMP390 Datasheet, Tabelle: Sleep mode current |
| Forced Mode (1× Messung) | 720 µA × 2 ms = **0.00144 mC** | Bosch BMP390 Datasheet |

> **Im Firmware nicht implementiert** — bleibt die gesamte Zeit im Sleep.

### LC709203F (Onsemi)
| Zustand | Strom | Quelle |
|---|---|---|
| Sleep (POWERMODE = 0x0002) | **1.5 µA** | Onsemi LC709203F Datasheet, Standby Current |
| Operate + Lesen | 500 µA × 5 ms = **0.0025 mC** | Onsemi LC709203F Datasheet + `lc709203f.c:193` (3 ms Delay) |

### Spannungsregler
| Zustand | Strom | Quelle |
|---|---|---|
| Immer (konstant) | **0.5 µA** | Angabe Benutzer |

---

## 2. Timing-Grundlage (aus Firmware-Analyse)

**15-Minuten-Super-Zyklus** (= 1 Wiederholeinheit, 3 × 5 min):

| Ereignis | Häufigkeit | MCU-Aktiv-Zeit |
|---|---|---|
| Kurz-TX (Cycle 0, FPort 2: T+H+Vbat) | 1× pro 15 min | ~1 818 ms |
| Kurz-TX (Cycle 1, FPort 2: T+H+Vbat) | 1× pro 15 min | ~1 818 ms |
| PreWake (SCD41 CO₂-Messung starten) | 1× pro 15 min | ~33 ms |
| SCD41 CO₂-Messung (autonom, MCU in STOP2) | 1× pro 15 min | 5 000 ms (MCU schläft!) |
| Voll-TX (Cycle 2, FPort 3: T+H+Vbat+CO₂) | 1× pro 15 min | ~1 898 ms |

**Gesamt MCU-Aktivzeit:** 2 × 1 818 + 33 + 1 898 = **5 567 ms** pro 15-min-Zyklus  
**STOP2-Schlafzeit:** 900 000 − 5 567 = **894 433 ms** pro 15-min-Zyklus  
*(enthält die 5 000 ms autonome SCD41-Messung, während der der MCU schläft)*

### Kurz-TX Phasen-Aufschlüsselung (je Ereignis)
| Phase | Dauer | Aktive Komponenten |
|---|---|---|
| SCD41 wake_up Delay | 30 ms | MCU + SCD41 |
| SCD41 MEASURE_SINGLE_SHOT_RHT | 50 ms | MCU + SCD41 |
| SCD41 Messung lesen + POWER_DOWN | 8 ms | MCU |
| LC709203F OPERATE + Delay + Lesen + SLEEP | 5 ms | MCU + LC709203F |
| MAC-Overhead + LmHandlerSend | 5 ms | MCU |
| **LoRa TX** (FPort 2, 19 Byte PHY, SF12/BW125) | **1 320 ms** | Radio + MCU |
| LoRa RX1-Fenster inkl. Vorbereitung | 200 ms | Radio + MCU |
| LoRa RX2-Fenster inkl. Vorbereitung | 200 ms | Radio + MCU |
| **Summe** | **1 818 ms** | |

### PreWake Phasen-Aufschlüsselung
| Phase | Dauer | Aktive Komponenten |
|---|---|---|
| SCD41 wake_up Delay | 30 ms | MCU + SCD41 |
| SCD41 MEASURE_SINGLE_SHOT senden + BusDeInit | 3 ms | MCU |
| **Summe MCU aktiv** | **33 ms** | |
| SCD41 CO₂-Messung (autonom) | 5 000 ms | **nur SCD41** (MCU in STOP2) |

### Voll-TX Phasen-Aufschlüsselung
| Phase | Dauer | Aktive Komponenten |
|---|---|---|
| SCD41 WaitDataReady + READ_MEASUREMENT + POWER_DOWN | 8 ms | MCU |
| LC709203F OPERATE + Delay + Lesen + SLEEP | 5 ms | MCU + LC709203F |
| MAC-Overhead + LmHandlerSend | 5 ms | MCU |
| **LoRa TX** (FPort 3, 21 Byte PHY, SF12/BW125) | **1 480 ms** | Radio + MCU |
| LoRa RX1-Fenster inkl. Vorbereitung | 200 ms | Radio + MCU |
| LoRa RX2-Fenster inkl. Vorbereitung | 200 ms | Radio + MCU |
| **Summe** | **1 898 ms** | |

---

## 3. Ladungsrechnung pro 15-Minuten-Zyklus

Formel: **Q [mAh] = I [mA] × t [ms] / 3 600 000**

### 3.1 STOP2-Schlaf-Strom

| Komponente | Strom (µA) |
|---|---|
| STM32WL STOP2 + RTC | 1.07 |
| SCD41 power_down | ~0.00 |
| SHT45 Deep Sleep | 0.10 |
| BMP390 Sleep | 2.00 |
| LC709203F Sleep | 1.50 |
| Spannungsregler | 0.50 |
| **Gesamt I_sleep** | **5.17 µA** |

```
Q_sleep = 0.00517 mA × 894 433 ms / 3 600 000 = 0.001284 mAh
```

### 3.2 Kurz-TX (je Ereignis, ×2 pro Zyklus)

```
Phase a — SCD41 RHT + MCU:
  I = 5 mA (MCU) + 2 mA (SCD41) = 7 mA
  t = 88 ms
  Q = 7 × 88 / 3 600 000 = 0.0001711 mAh

Phase b — LC709203F + MCU:
  I = 5 mA (MCU) + 0.5 mA (LC709203F) = 5.5 mA
  t = 5 ms
  Q = 5.5 × 5 / 3 600 000 = 0.0000076 mAh

Phase c — MAC-Overhead:
  I = 5 mA (MCU)
  t = 5 ms
  Q = 5 × 5 / 3 600 000 = 0.0000069 mAh

Phase d — LoRa TX:
  I = 22 mA (Gesamt-Chip @ 14 dBm LP PA)
  t = 1 320 ms
  Q = 22 × 1 320 / 3 600 000 = 0.0080667 mAh

Phase e — LoRa RX1:
  I = 4.82 mA (Gesamt-Chip)
  t = 200 ms
  Q = 4.82 × 200 / 3 600 000 = 0.0002678 mAh

Phase f — LoRa RX2:
  I = 4.82 mA
  t = 200 ms
  Q = 4.82 × 200 / 3 600 000 = 0.0002678 mAh

─────────────────────────────────────────────────
Q_kurz_tx = 0.0001711 + 0.0000076 + 0.0000069
          + 0.0080667 + 0.0002678 + 0.0002678
          = 0.008788 mAh

× 2 Ereignisse: Q_kurz_tx_total = 0.017576 mAh
```

### 3.3 PreWake (×1 pro Zyklus)

```
Phase a — MCU + SCD41 wake_up:
  I = 5 mA (MCU) + 2 mA (SCD41 während Wakeup) = 7 mA
  t = 33 ms
  Q = 7 × 33 / 3 600 000 = 0.0000642 mAh

Q_prewake = 0.0000642 mAh
```

### 3.4 SCD41 CO₂-Messung (autonom, MCU in STOP2, ×1 pro Zyklus)

```
Ladung pro Shot (Firmware Steady-State, 1 Shot):
  q_ss = 77 mC = 77 mAs = 77 / 3 600 mAh

Q_co2 = 77 / 3 600 = 0.021389 mAh

Mittlerer Strom während Messung:
  I_avg_SCD41 = 77 mC / 5 000 ms = 15.4 mA (pulsförmig, MCU schläft)

Konservativ (q_pcss = 154 mC, 2 Shots pro Zyklus):
  Q_co2_konservativ = 154 / 3 600 = 0.042778 mAh
```

### 3.5 Voll-TX (×1 pro Zyklus)

```
Phase a — SCD41 Daten lesen (Messung war fertig):
  I = 5 mA (nur MCU, SCD41 fertig gemessen)
  t = 8 ms
  Q = 5 × 8 / 3 600 000 = 0.0000111 mAh

Phase b — LC709203F + MCU:
  I = 5.5 mA
  t = 5 ms
  Q = 5.5 × 5 / 3 600 000 = 0.0000076 mAh

Phase c — MAC-Overhead:
  I = 5 mA
  t = 5 ms
  Q = 5 × 5 / 3 600 000 = 0.0000069 mAh

Phase d — LoRa TX:
  I = 22 mA
  t = 1 480 ms  (FPort 3, 21 Byte PHY, SF12)
  Q = 22 × 1 480 / 3 600 000 = 0.0090444 mAh

Phase e — LoRa RX1:
  I = 4.82 mA
  t = 200 ms
  Q = 4.82 × 200 / 3 600 000 = 0.0002678 mAh

Phase f — LoRa RX2:
  Q = 0.0002678 mAh

─────────────────────────────────────────────────
Q_voll_tx = 0.0000111 + 0.0000076 + 0.0000069
          + 0.0090444 + 0.0002678 + 0.0002678
          = 0.009606 mAh
```

---

## 4. Gesamtrechnung pro 15-Minuten-Zyklus

| Posten | Ladung (mAh) | Anteil |
|---|---|---|
| STOP2-Schlaf (alle Komponenten) | 0.001284 | 2.6 % |
| Kurz-TX × 2 (MCU + SCD41 RHT + LC709203F + LoRa) | 0.017576 | 35.2 % |
| PreWake (MCU + SCD41 wecken) | 0.000064 | 0.1 % |
| **SCD41 CO₂-Messung** (MCU schläft, SCD41 aktiv) | **0.021389** | **42.8 %** |
| Voll-TX (MCU + SCD41 lesen + LC709203F + LoRa) | 0.009606 | 19.2 % |
| **Gesamt pro 15-min-Zyklus** | **0.049919 mAh** | 100 % |

---

## 5. Stunden-, Tages- und Akku-Berechnung

```
Zyklen pro Stunde : 60 / 15 = 4
Zyklen pro Tag    : 24 × 4 = 96

Pro Stunde:
  Q_h = 4 × 0.049919 mAh = 0.19968 mAh/h ≈ 0.200 mAh/h

Mittlerer Strom:
  I_avg = 0.200 mAh/h = 200 µA

Pro Tag:
  Q_d = 96 × 0.049919 = 4.792 mAh/Tag

Akkulaufzeit (1 000 mAh Nennkapazität):
  t = 1 000 mAh / 0.200 mAh/h = 5 000 h = 208.3 Tage ≈ 6.9 Monate
```

---

## 6. Dominante Verbraucher (Übersicht)

| Verbraucher | mAh/h | Anteil am Stundenwert |
|---|---|---|
| SCD41 CO₂-Messung (4× / h) | 0.0856 mAh/h | **42.8 %** |
| LoRa TX (12 Frames/h: 8× FPort 2 + 4× FPort 3) | 0.1007 mAh/h | **50.4 %** |
| LoRa RX-Fenster (12× RX1 + 12× RX2, je 200 ms) | 0.0064 mAh/h | 3.2 % |
| STOP2-Schlaf (alle Komponenten) | 0.0051 mAh/h | 2.6 % |
| MCU aktiv (Sensor-Reads, Overhead) | 0.0018 mAh/h | 0.9 % |
| SHT45 Deep Sleep | 0.0001 mAh/h | 0.05 % |
| BMP390 Sleep | 0.0002 mAh/h | 0.1 % |
| LC709203F Sleep | 0.0002 mAh/h | 0.1 % |
| LC709203F Reads | < 0.0001 mAh/h | < 0.05 % |
| SCD41 RHT-Shots (8× / h) | 0.0004 mAh/h | 0.2 % |
| Spannungsregler | < 0.0001 mAh/h | < 0.05 % |
| **Gesamt** | **0.200 mAh/h** | **100 %** |

---

## 7. Szenarien-Vergleich

| Szenario | I_avg | Laufzeit (1 000 mAh) |
|---|---|---|
| **DR0 (SF12) — Standard, schlechtestes Signal** | **200 µA** | **208 Tage (~6.9 Mon.)** |
| DR5 (SF7) — gutes Signal, ADR aktiv ¹ | ~97 µA | ~429 Tage (~14.3 Mon.) |
| DR0, konservativ (q_pcss = 154 mC für CO₂) ² | ~248 µA | ~168 Tage (~5.6 Mon.) |
| DR5, konservativ | ~145 µA | ~287 Tage (~9.6 Mon.) |

> ¹ Bei DR5 (SF7/BW125): TX-Zeit sinkt auf ~51 ms (FPort 2) / ~57 ms (FPort 3).  
>   LoRa TX-Anteil: 22 mA × (51×8 + 57×4) / 3 600 000 = 22 × 636 / 3 600 000 = 0.00389 mAh/h  
>   SCD41 CO₂ dominiert dann mit ~88 % des Verbrauchs.
>
> ² Konservativ: 154 mC pro CO₂-Messung (Datenblatt-Wert für Power-Cycled mit 2 Shots).

---

## 8. Annahmen und Einschränkungen

- **LoRa TX 14 dBm:** Strom von 22 mA interpoliert aus Datenblatt (15 mA @ 10 dBm, LP PA max ~15 dBm).
  Exakter Wert aus DS13105 Tabelle 24/25 (nicht vollständig vorliegend).
- **SCD41 RHT-Shot:** Strom von ~2 mA geschätzt (kein CO₂-IR-Emitter aktiv); exakter Wert nicht
  im vorliegenden App Note dokumentiert.
- **ADR-Effekt:** Bei gutem Signal aktiviert ADR höhere Datenraten (DR1–DR5), was die LoRa-TX-Zeit
  drastisch reduziert. Das entlastet den LoRa-Anteil, während SCD41 CO₂ konstant bleibt.
- **SHT45 / BMP390:** Aktuell nicht im Firmware-Code implementiert; verbleiben dauerhaft im Sleep.
- **RX-Fenster:** Zwischen TX-Ende und RX1 (+1 000 ms) sowie RX1–RX2 (~700 ms) schläft der MCU
  in STOP2 (RTC-Alarm). Nur die eigentlichen Fenster (~200 ms inkl. Vorbereitung) zählen als aktiv.
- **Kein Downlink:** Wenn der Network-Server einen Downlink sendet (RX1 trifft), entfällt RX2 und
  der Verbrauch sinkt geringfügig (~0.003 mAh/h weniger).
- **Temperatur:** Alle Werte bei 25 °C. Bei −20 °C (Außeneinsatz) steigt der Schlafstrom des STM32
  leicht; die Kapazität des Akkus sinkt signifikant (~20–40 % bei LiPo).
- **Selbstentladung Akku:** 1–3 % / Monat (LiPo); bei 1 000 mAh und 6.9 Monaten Laufzeit
  geht ca. 70–200 mAh durch Selbstentladung verloren → reale Laufzeit ca. **6.2–6.6 Monate**.
