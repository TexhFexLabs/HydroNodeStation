# HydroNodeStation: Backend-Übergabe und Kalibrierungsstatus

Stand: 06.09.2026. Protokollbeschreibung für Firmware 1.6 auf `feature/long-term-reliability`, ausgehend von Commit `952df1f`. Die Firmware-Änderungen liegen auf dem Branch `feature/long-term-reliability`. Diese Übergabe beschreibt den vorhandenen Sender und die noch nötige Backend-Arbeit. Das produktive HydroNode-Backend wurde hier nicht geändert; die Station wurde nicht geflasht.

## 1. Was bei funktionierenden Sensoren gleich bleibt

Der Datenweg bleibt **Wetterstation → LoRaWAN → TTN → HTTP-Webhook → HydroNode-Netzwerk**. Die Station sendet binäre Messwerte. Ein JavaScript-Decoder im Firmware-Repository läuft nicht automatisch in diesem Datenweg.

Bei gleicher Konfiguration und gültigen Werten bleiben fPort 2/3/4, Paketlängen, Byte-Reihenfolge, Skalierung und Einheiten unverändert. Das bedeutet: Ein Backend, das diese gültigen Messwerte bisher korrekt aus den Bytes gelesen hat, kann sie weiterhin genauso lesen. Die Fehlerbehebung verändert weder Temperatur- noch CO2-Skalierung.

Die Aussage bedeutet nicht, dass Funkempfang und Messqualität garantiert sind. Ausnahmen gegenüber früher: neue Kennwerte für nicht verfügbare Messungen, optionaler Diagnoseport 5, Batterie-Erholungsmodus und robusterer Neustart/Join. Bei normaler Akkuspannung bleibt das Standardintervall 180 Sekunden. Bei fehlenden Messwerten darf der alte Decoder die neuen Kennwerte nicht als Zahlen abspeichern.

| Eigenschaft | Bestehendes Verhalten / Firmware 1.6 |
|---|---|
| Basisdaten | fPort 2, 10 Byte |
| Basisdaten + CO2 | fPort 3, 12 Byte |
| Basisdaten + CO2 + Partikel | fPort 4, 32 Byte |
| Standardabstand | 180 s; per Downlink 30–3600 s konfigurierbar |
| CO2 / Partikel | Jeder 5. / 10. Mess-/Sendezyklus, standardmäßig etwa 15 / 30 min |
| Fehlende Werte früher | Häufig `0`; nicht sicher von einer echten Null unterscheidbar |
| Fehlende Werte 1.6 | Temperatur `0x8000`, andere Messfelder `0xFFFF` |
| Diagnose | fPort 5, 22 Byte, nur auf Anfrage |

Der Zykluszähler zählt Aufrufe des Mess-/Sendeablaufs, keine nachgewiesenen Zustellungen im Backend. Paketverlust, Join-Zeit und Neustart können daher die im Backend sichtbare Portfolge und Abstände verändern.

## 2. Wo der Backend-Umbau ansetzt

Laut Studiendokumentation wird die Station über DevEUI im `SensorLookupCache` gesucht; `LoRaFPortConfig` liefert die Binärdekodierung. Danach folgt `hydronode.verified.sensor` → `SensorValueProcessor` → Datenbank. Das ist die dokumentierte Architektur, kein in diesem Repository verifizierter aktueller Backend-Quellstand.

Der Umbau gehört an die Stelle, an der der TTN-Webhook in HydroNode dekodiert wird, und in die nachfolgende Messwert-/Qualitätsverarbeitung. TTN muss dafür nicht durch einen anderen Dienst ersetzt werden. Ein TTN-Payload-Formatter ist nicht erforderlich, solange HydroNode selbst `frm_payload` liest.

### TTN-Eingangsfelder

| JSON-Pfad | Verarbeitung |
|---|---|
| `end_device_ids.dev_eui` | Station identifizieren; konsistentes Hex-Format, führende Nullen erhalten |
| `end_device_ids.application_ids.application_id` | Anwendungszuordnung gemäß bestehender Integration |
| `uplink_message.f_port` | Zuerst auf Mess-/Diagnoseformat verzweigen |
| `uplink_message.frm_payload` | Base64 → Bytes → Längenprüfung → Dekodierung |
| `uplink_message.f_cnt` | Transportmetadatum, nicht der Firmware-Messzykluszähler |
| `uplink_message.session_key_id` | Sessionkontext für Deduplizierung, wenn vorhanden |
| `uplink_message.received_at` / `received_at` | UTC-Empfangszeit gemäß bestehendem Backend-Zeitmodell |
| `uplink_message.decoded_payload` | Optional; nicht voraussetzen und nicht zusätzlich zu den Rohbytes nochmals skalieren |
| `correlation_ids`, `rx_metadata` | Optional für Nachvollziehbarkeit, Empfangsqualität und Deduplizierung |

The Things Stack beschreibt diese Felder in seinem [Datenformat](https://www.thethingsindustries.com/docs/integrations/data-formats/). Leere/defaultwertige Felder können entfallen; insbesondere darf ein nicht übertragenes `f_cnt` mit Default 0 nicht mit einem komplett fehlenden Uplink verwechselt werden. Andere Ereignisse wie Join-/Downlink-Bestätigungen sind keine Umweltmessungen.

Ein minimales, synthetisches Uplink-Beispiel:

```json
{
  "end_device_ids": {
    "device_id": "station-test",
    "application_ids": {"application_id": "hydronode-test"},
    "dev_eui": "0000000000000001"
  },
  "received_at": "2026-09-06T12:00:00Z",
  "uplink_message": {
    "f_port": 2,
    "f_cnt": 42,
    "frm_payload": "CGYVfCeUEGgBRQ=="
  }
}
```

Erwartet: 21,50 °C, 55,00 %RH, 1013,2 hPa, 4200 mV, UVI 3,25. Kennung und Zeit sind Testdaten, keine echte Station.

## 3. Verbindliches Binärformat für Messwerte

Alle Offsets sind nullbasiert. Jedes Feld ist zwei Byte lang, **Big-Endian**. Einzige signed Messgröße: Temperatur als Zweierkomplement `int16`. Die Skalierung erfolgt genau einmal nach der Sentinelprüfung.

| Byteoffset | Referenzname | Typ | Physikalischer Wert | Enthalten auf fPort |
|---:|---|---|---|---|
| 0 | `temperature` | i16 | Rohwert / 100 °C | 2, 3, 4 |
| 2 | `humidity` | u16 | Rohwert / 100 %RH | 2, 3, 4 |
| 4 | `pressure` | u16 | Rohwert / 10 hPa, absoluter Stationsdruck | 2, 3, 4 |
| 6 | `battery_mv` | u16 | Rohwert mV | 2, 3, 4 |
| 8 | `uvi` | u16 | Rohwert / 100 | 2, 3, 4 |
| 10 | `co2_ppm` | u16 | Rohwert ppm | 3, 4 |
| 12 | `pm1_0` | u16 | Rohwert / 10 µg/m³ | 4 |
| 14 | `pm2_5` | u16 | Rohwert / 10 µg/m³ | 4 |
| 16 | `pm4_0` | u16 | Rohwert / 10 µg/m³ | 4 |
| 18 | `pm10` | u16 | Rohwert / 10 µg/m³ | 4 |
| 20 | `nc0_5` | u16 | Rohwert / 10 #/cm³ | 4 |
| 22 | `nc1_0` | u16 | Rohwert / 10 #/cm³ | 4 |
| 24 | `nc2_5` | u16 | Rohwert / 10 #/cm³ | 4 |
| 26 | `nc4_0` | u16 | Rohwert / 10 #/cm³ | 4 |
| 28 | `nc10` | u16 | Rohwert / 10 #/cm³ | 4 |
| 30 | `typ_size_um` | u16 | Rohwert / 1000 µm | 4 |

Die Namen sind die Referenznamen aus [payload-decoder.js](payload-decoder.js). Bestehende HydroNode-Kanal-IDs können unverändert bleiben; ihre Zuordnung muss dieselben Größen und Einheiten ergeben. Die reale Stationstemperatur/-feuchte stammt vom **SHT45**, nicht aus dem SCD41-Temperatur-/Feuchtekanal.

**Port 4 bleibt immer 32 Byte lang.** Ist SCD41 deaktiviert oder seine Messung fehlgeschlagen, bleibt sein Zwei-Byte-Platz an Offset 10 erhalten und enthält `FFFF`. Die PM-Werte werden nicht vorgezogen. Auf Port 2 fehlt CO2 hingegen planmäßig vollständig; das ist kein CO2-Fehler.

## 4. Spezielle Fehlerwerte: zuerst erkennen, dann umrechnen

| Feldgruppe | Bytes | Bedeutung für HydroNode |
|---|---|---|
| Temperatur | `80 00` | Kein verwendbarer Temperaturwert; `null`/fehlend |
| Alle unsigned **Messfelder** | `FF FF` | Reservierter/nicht verfügbarer Messwert; `null`/fehlend |
| Temperatur | `FF FF` | **Gültig: −0,01 °C**, kein Fehlerkennwert |
| Temperatur | `FE 0C` | Gültig: −5,00 °C |
| Messfelder | `00 00` | Nicht pauschal verwerfen: z.B. UV nachts oder 0 °C möglich |
| Diagnosezähler auf Port 5 | `FF FF` | **65535**, ggf. gesättigter Zähler; hier kein Null-Sentinel |

Implementierungsreihenfolge:

```text
raw = (unsigned(byte0) << 8) | unsigned(byte1)
if temperature and raw == 0x8000: missing
else if unsigned_measurement and raw == 0xFFFF: missing
else:
    signed_temperature = raw >= 0x8000 ? raw - 0x10000 : raw
    apply exactly the field's scaling
```

Bei Java/Quarkus müssen Bytewerte mit `& 0xFF` und unsigned Shorts entsprechend behandelt werden; fehlende Werte brauchen eine nullable Darstellung oder einen getrennten Qualitätsstatus. Nicht auf bereits skalierte `655,35`, `6553,5` oder `−327,68` prüfen, wenn Rohbytes verfügbar sind. Die Prüfung gehört auch vor backendseitige Offsets, Einheitenwechsel und Kalibrierungskorrekturen.

### Was ein Sentinel aussagt – und was nicht

Ein Sentinel bedeutet **kein verwertbarer Wert für dieses Feld in diesem Paket**. Mögliche Ursachen: I2C-Fehler, CRC-Fehler, Sensor nicht initialisiert, CO2-Messung nicht bereit/Phase fehlgeschlagen oder absichtlich deaktivierter CO2-Kanal. Der Payload liefert keinen individuellen Fehlercode pro Feld. Deshalb nicht allein daraus „Sensor kaputt“ oder „Akku leer“ ableiten.

Zusätzliche Grenze des aktuellen Formats: Manche Treiber sättigen übergroße Werte auf `65535`; dieser Wert ist nun reserviert. Auch so ein Grenzwert wird im Backend fehlend behandelt. Die Firmware kann einen Sensorzugriff dabei intern als erfolgreich zählen. Ein Diagnose-Gültigkeitsbit ist deshalb kein Grund, einen Rohwert `FFFF` doch numerisch zu übernehmen.

Akku-Behandlung: Scheitert bereits die Spannungsprüfung vor dem Sendezyklus, kann die Firmware den ganzen Mess-Uplink auslassen. Ein Batterie-Sentinel ist also nicht garantiert die Vorwarnung vor jeder Funkpause. Drei fehlgeschlagene Spannungsprüfungen können RECOVERY auslösen.

## 5. Backend-Verarbeitung, Datenbank und Oberfläche

### Pflichtänderungen

| Priorität | Arbeit | Abnahmekriterium |
|---|---|---|
| P0 | Geräte-/Protokollprofil für diese Station und Firmware 1.6 | Sentinelregeln werden nicht blind auf fremde Geräte angewandt |
| P0 | fPort-/Längenprüfung: 2→10, 3→12, 4→32, 5→22 | Nie fehlende Bytes automatisch als Null interpretieren |
| P0 | Sentinelprüfung auf Rohwerten; Temperatur signed | `FFFF` bei Temperatur ist −0,01, bei UV fehlend |
| P0 | Gültige Teilmessungen übernehmen | Fehlendes CO2 verwirft nicht Temperatur, Druck und PM desselben Pakets |
| P0 | Fehlende Werte nicht als Messpunkt in numerische Auswertung geben | Keine Ausreißer, Mittelwerte, Min/Max oder KI-Alarme aus Sentinels |
| P0 | Port 5 separat behandeln | Kein Diagnosepaket als Temperatur-/PM-Messung dekodieren |
| P0 | „Station gehört“ und „Kanal gültig gemessen“ getrennt halten | Sentinel-Uplink aktualisiert Empfang, aber nicht den Zeitpunkt des letzten gültigen Kanalwerts |
| P1 | Null-/Qualitätsstatus bis API, Diagramm und Export erhalten | Kein `null → 0`; Datenlücke/staler Wert klar erkennbar |
| P1 | Uplink-Duplikate idempotent behandeln | Derselbe Webhook erzeugt keine doppelten Messpunkte |
| P1 | Kanalabhängige Ausfall-/Frischefristen | Kein CO2-Ausfallalarm nach drei Minuten bei normalem 15-Minuten-Takt |

Empfohlenes logisches Modell (Vorschlag für den Backend-Umbau, keine bereits vorhandenen Tabellen): Rohereignis unverändert aufbewahren; pro enthaltenem Kanal entweder Wert + `valid` oder `null` + `unavailable` erfassen. Für nicht im Port enthaltene Kanäle keinen neuen Messwert erzeugen (`not_sampled`/keine Beobachtung). Physikalisch unplausible Zahlen separat markieren (`out_of_range`); das ist ein anderer Zustand als der Sentinel.

Wenn die bestehende Messwerttabelle keine Nullwerte erlaubt: nur gültige numerische Messpunkte schreiben und fehlgeschlagene Beobachtungen separat als Qualitätsereignis ablegen. Den letzten gültigen Wert darf die UI weiter anzeigen, aber mit seinem ursprünglichen Zeitstempel und einem Hinweis auf Veraltung. Niemals künstlich einen neuen „gültigen“ Messpunkt aus dem vorherigen Wert erzeugen.

In der dokumentierten Kafka-Strecke muss diese Trennung spätestens **vor** `SensorValueProcessor`, Aggregation und KI-Auswertung greifen. „verified“ im Topicnamen darf nicht mit metrologisch kalibriert oder jedem Feld gültig gleichgesetzt werden. Versions-/Portkonfigurationen und der dokumentierte `FPortConfigCache` müssen nach der Umstellung aktualisiert/invalidiert werden; nicht auf eine möglicherweise lange TTL warten.

Ein gültiger Webhook mit einzelnen Sensorfehlern ist kein HTTP-Transportfehler. Nach erfolgreicher Übernahme gemäß bestehendem Durability-Konzept normal bestätigen. Temporäre Speicher-/Verarbeitungsfehler nach vorhandener Retry-Strategie behandeln. Ungültiges Base64, unmögliche Paketlängen oder unbekannte Ports gesondert protokollieren/quarantänisieren, niemals als Nullmessung akzeptieren.

### Deduplizierung und Neustarts

Vorschlag: stabiler Ereignisschlüssel aus Gerätezuordnung, Sessionkontext, Frame Counter und Port, passend zum vorhandenen Integrationsmodell. Nicht allein `DevEUI + f_cnt` verwenden: Firmware macht nach Neustart wieder OTAA; Session/Frame-Counter-Verlauf kann wechseln. Fehlt der Sessionkontext, braucht es eine dokumentierte Alternative mit stabiler Ereignisidentität, nicht einfach den Rohpayload-Hash: aufeinanderfolgende echte Messungen können identische Bytes haben.

`boot_count` ist ebenfalls keine globale eindeutige Session-ID: RTC-Backupdaten können nach vollständigem Stromverlust verloren gehen. Nach einem Neustart beginnt der Firmware-Messzykluszähler erneut; erst später erscheinen wieder CO2-/PM-Pakete.

## 6. Diagnoseport 5: exakt 22 Byte

Downlink auf **fPort 2**, ein Byte **`12` hex** (dezimal 18, Base64 `Eg==`) fordert Diagnosen an. Das ist kein ASCII-Text `"12"`. Die Firmware versucht einen unbestätigten Uplink auf fPort 5. Es gibt keine garantierte Antwort: Downlink-Zustellung, Funkverlust, RECOVERY und Modem-Busy können sie verhindern. Während RECOVERY werden keine regulären Uplinks/RX-Fenster für solche Anfragen bereitgestellt.

| Offset | Bytes | Typ | Feld / Bedeutung |
|---:|---:|---|---|
| 0 | 1 | u8 | Diagnoseformat-Version, aktuell **1**; nicht Firmwareversion 1.6 |
| 1 | 1 | u8 | Power mode: 0 NORMAL, 1 SAVE, 2 RECOVERY |
| 2 | 4 | u32 | MCU-/RTC-Sekunden; Referenzdecoder nennt das `uptime_s` |
| 6 | 4 | u32 | `boot_count` |
| 10 | 4 | u32 | `reset_flags`: gespeicherter roher RCC_CSR-Wert beim Start |
| 14 | 2 | u16 | `last_fault`: letzter explizit gespeicherter Firmwarefehler |
| 16 | 2 | u16 | `tx_errors`: fehlgeschlagene Uplink-Anfragen seit Boot, sättigt bei 65535 |
| 18 | 2 | u16 | `sensor_errors`: aufsummierte Fehlerereignisse seit Boot, sättigt bei 65535 |
| 20 | 2 | u16 | `sensor_valid_mask`: Gruppenstatus des letzten Messablaufs |

Alle Mehrbytewerte sind Big-Endian unsigned. Für u32 in Java `long`/unsigned Konvertierung, in JavaScript nicht versehentlich ein negatives signed Bitoperationsergebnis verwenden. Alle uint32-Werte passen exakt in JavaScript Number.

`uptime_s` ist **kein Unix-Zeitstempel** und keine garantierte Zeit seit dem letzten MCU-Reset: die RTC kann weiterlaufen, ihre Software-Erweiterung wird bei Init neu aufgebaut. Für historische Messzeitpunkte TTN-/Backend-Empfangszeit verwenden. Die Umweltpakete tragen keinen eigenen Messzeitstempel; langsame Sensoren messen schon vor dem Uplink.

### last_fault

| Wert | Bedeutung |
|---:|---|
| 0 | Kein expliziter Fehler verfügbar; schließt Hardware-Watchdog/Spannungsreset nicht aus |
| 1 | HAL-Fehler |
| 2 | CPU-/Exception-Fehler |
| 3 | Modem-/Funkablauffehler |
| 4 | Erwarteter Anwendungsfortschritt ausgeblieben |
| 5 | NVM-/Persistenzfehler |

`last_fault` wird in RTC-Backupregistern gehalten und beim nächsten Boot als letzter Grund übernommen. Bei vollständig verlorener Versorgung kann dieser Verlauf fehlen. Er ist kein per-Messfeld-Fehlercode und kein Kalibrierungsstatus.

### reset_flags (STM32WLE5)

Der Rohwert enthält neben Resetflags andere RCC-Bits. Bits maskieren; nicht den ganzen Wert auf Gleichheit testen. Mehrere Flags können gleichzeitig gesetzt sein.

| Maske | CMSIS-Flag |
|---|---|
| `0x00004000` | RFRSTF |
| `0x01000000` | RFILARSTF |
| `0x02000000` | OBLRSTF |
| `0x04000000` | PINRSTF |
| `0x08000000` | BORRSTF |
| `0x10000000` | SFTRSTF |
| `0x20000000` | IWDGRSTF |
| `0x40000000` | WWDGRSTF |
| `0x80000000` | LPWRRSTF |

BORRSTF allein beweist keinen defekten Akku; Power-on/Resetkontext mitbetrachten. IWDGRSTF mit `last_fault=0` ist möglich: ein Hardware-Watchdog muss vor seinem Reset keinen Softwarefehler gespeichert haben. Maßgeblich ist der im Projekt mitgelieferte STM32WLE5-CMSIS-Header.

### sensor_valid_mask

| Bit | Maske | Gruppe |
|---:|---|---|
| 0 | `0x0001` | Batterie |
| 1 | `0x0002` | SHT45 Temperatur **und** Feuchte |
| 2 | `0x0004` | BMP390 Druck |
| 3 | `0x0008` | LTR390 UV |
| 4 | `0x0010` | SCD41 CO2 |
| 5 | `0x0020` | SPS30: alle zehn Partikelfelder |

Beispiele: vollständig gültige Basisdaten `0x000F`, mit CO2 `0x001F`, kompletter Port 4 `0x003F`. Bit 4/5 = 0 kann lediglich bedeuten, dass im letzten Basiszyklus kein CO2/PM vorgesehen war. Die Maske gehört nicht als Zusatzbyte an Port 2/3/4 und ist nicht zwingend zeitgleich mit dem Diagnosepaket. Für konkrete Messfelder sind die Rohwerte ihres jeweiligen Messpakets entscheidend. Ein gesetztes Bit besagt weder „kalibriert“ noch „physikalisch garantiert richtig“.

## 7. Weitere Downlinks und erwartete Funkpausen

| Downlink fPort 2 | Bedeutung | Backend-Besonderheit |
|---|---|---|
| `10 HH LL` | Neues Basisintervall in Sekunden, u16 BE, 30–3600; Speicherung vor Übernahme | Keine gesonderte Konfigurationsbestätigung im aktuellen Protokoll; nur erfolgreicher TTN-Transport beweist noch keine Anwendung |
| `11` | SPS30-Lüfterreinigung | Keine Kalibrierung; bei unpassendem Akku-Zustand abgelehnt/ignoriert |
| `12` | Diagnoseanforderung | Antwort gegebenenfalls Port 5 |
| `FF` | MCU-Software-Reset | Neuer Join/Messzyklus möglich; kein SCD41-Factory-Reset |

Im SAVE-Zustand verdoppelt sich das Basisintervall; im RECOVERY-Zustand pausieren Mess-/Funkaktivitäten. Schwellen und Wiederanlauf sind in [RELIABILITY.md](RELIABILITY.md) dokumentiert. Das Backend kann aus reiner Funkstille **nicht sicher** zwischen Energiesparen, leerem Akku, Funkloch, Gateway-Ausfall und Gerätedefekt unterscheiden. Allenfalls eine begründete Vermutung aus dem letzten Verlauf anzeigen.

Die normalen Umwelt-Uplinks sind unbestätigt. Ein erfolgreicher lokaler Sendeabschluss ist kein Zustellbeweis im HydroNode-Backend. Die Station speichert derzeit keine vollständige Messhistorie zur späteren Nachlieferung; Pausen können echte Datenlücken hinterlassen.

## 8. Kopierbare Testvektoren

Die folgenden Frames sind synthetisch. Hex beschreibt den Byteinhalt nach Base64-Dekodierung. Alle Vektoren wurden mit dem mitgelieferten Referenzdecoder geprüft; ihre Erwartungen sind die Abnahmebasis für den tatsächlichen Backend-Decoder.

### Gültige Basiswerte

```text
fPort: 2
Hex:    0866157c279410680145
Base64: CGYVfCeUEGgBRQ==
Erwartet: temperature=21.5, humidity=55, pressure=1013.2, battery_mv=4200, uvi=3.25
```

### Gültige Basiswerte plus CO2

```text
fPort: 3
Hex:    0866157c27941068014501c2
Base64: CGYVfCeUEGgBRQHC
Erwartet: gleiche Basiswerte, co2_ppm=450
```

### Vollständige Messung

```text
fPort: 4
Hex:    0866157c27941068014501c2000a0017002d0043006400c8012c019001f401f4
Base64: CGYVfCeUEGgBRQHCAAoAFwAtAEMAZADIASwBkAH0AfQ=
Erwartet: gleiche Basiswerte + CO2;
pm1_0=1, pm2_5=2.3, pm4_0=4.5, pm10=6.7,
nc0_5=10, nc1_0=20, nc2_5=30, nc4_0=40, nc10=50, typ_size_um=0.5
```

### Negative Temperatur und Temperatur-FFFF-Sonderfall

```text
fPort: 2
Hex:    fe0c157c279410680145
Base64: /gwVfCeUEGgBRQ==
Erwartet: temperature=-5, andere Basiswerte unverändert

fPort: 2
Hex:    ffff157c279410680145
Base64: //8VfCeUEGgBRQ==
Erwartet: temperature=-0.01, nicht null
```

### Fehlendes CO2 und ausschließlich fehlende Basisfelder

```text
fPort: 3
Hex:    0866157c279410680145ffff
Base64: CGYVfCeUEGgBRf//
Erwartet: Basiswerte gültig, co2_ppm=null

fPort: 2
Hex:    8000ffffffffffffffff
Base64: gAD//////////w==
Erwartet: alle fünf Basisfelder null; keine numerischen Messpunkte erzeugen
```

### PM-Gruppe fehlt; Null-UV bleibt gültig

```text
fPort: 4
Hex:    0866157c27941068014501c2ffffffffffffffffffffffffffffffffffffffff
Base64: CGYVfCeUEGgBRQHC//////////////////////////8=
Erwartet: Basiswerte/CO2 gültig, alle zehn Partikelfelder null

fPort: 2
Hex:    0866157c279410680000
Base64: CGYVfCeUEGgAAA==
Erwartet: uvi=0, kein Fehler; andere Basiswerte gültig
```

### Diagnose mit gesättigtem Fehlerzähler

```text
fPort: 5
Hex:    01010041895800000007200000000000ffff0003000f
Base64: AQEAQYlYAAAAByAAAAAAAP//AAMADw==
Erwartet: version=1, power_mode=1, uptime_s=4295000, boot_count=7,
reset_flags=536870912 (=0x20000000), last_fault=0,
tx_errors=65535 (nicht null), sensor_errors=3, sensor_valid_mask=15
```

Zusätzliche Backend-Tests: falsche Länge bei jedem Port, ungültiges Base64, unbekannter Port/Diagnoseversion, Paket mit gültigem PM und fehlendem CO2, doppelt zugestellter Webhook, Frame Counter 0 nach neuer Session, identische echte Messwerte in verschiedenen Frames, reiner Join-Webhook, CO2/PM planmäßig nicht enthalten. Diagnosefehler/Versionskonflikte dürfen keine Umweltmessung erzeugen.

## 9. Migration und Abnahme

1. Rohpayload-Decoder am realen TTN-Webhook identifizieren; keine zweite Dekodierung oder doppelte Skalierung einführen.
2. Tabellen/Sentinelregeln und Port-5-Verzweigung implementieren. Das JavaScript hier ist Referenz, keine notwendige Backend-Laufzeitabhängigkeit.
3. Gültige historische Rohpayloads durch alten und neuen Backend-Decoder laufen lassen. Für reguläre Werte müssen identische Ergebnisse entstehen. Eine bisher falsche unsigned Temperaturdekodierung ausdrücklich als Bugfix behandeln.
4. Obige Fehler-/Grenzfall-Vektoren bis Datenbank, API, Diagramm und Aggregation prüfen, nicht nur die Decoderfunktion.
5. Profil und Cache aktualisieren. Umweltports enthalten keine explizite Firmwareversion; Firmware 1.6 darf nicht allein aus fPort oder normalen Messwerten erraten werden. Diagnoseformat-Version 1 ist ebenfalls kein Firmware-Versionsnachweis.
6. Backend zuerst ausrollen, dann eine Station gezielt aktualisieren. Flash-Migration und den Erhalt der letzten 8 KiB gemäß RELIABILITY.md beachten.
7. Basis-, CO2-, PM- und optional Diagnose-Uplink end-to-end prüfen. Bei einem absichtlich fehlenden Sensor bleiben andere Kanäle verwertbar.
8. Erst anschließend weitere Geräte aktualisieren und den Langzeitlauf überwachen.

Alte `0`-Messwerte nicht pauschal nachträglich in null umschreiben: ohne zusätzliche Evidenz sind gültige Null und alter Fehler-Fallback nicht unterscheidbar. Bestehende historische UV- oder Temperatur-Korrekturen nicht nochmals auf bereits korrigierte neue Werte anwenden.

## 10. CO2: Kalibrierung, Stabilisierung und Neustart

### Aktueller Codezustand

| Funktion / Einstellung | Aktueller Stand |
|---|---|
| Messmodus | SCD41 Single-Shot mit `power_down`/`wake_up` zwischen Messzyklen |
| Erste Messung | Stabilisierung, wird verworfen |
| Zweite Messung | Nutzwert; Readiness und CRC geprüft |
| Fehler in der Messfolge | Kein gültiger Nutzwert; CO2 bleibt Sentinel |
| `SCD41_ASC_ENABLED` | `0`: ASC beim Init explizit deaktiviert |
| `SCD41_PERSIST_SETTINGS` | `0`: kein wiederholtes Schreiben der Sensorkonfiguration ins EEPROM |
| `SCD41_SENSOR_ALTITUDE_M` | `0`: Firmware sendet keine eigene Höhenvorgabe |
| `SCD41_AMBIENT_PRESSURE_MBAR` | `0`: Firmware sendet keine eigene Druckvorgabe |
| Dynamischer BMP390-Druck für SCD41 | Noch nicht implementiert |
| Manuelle Referenzkalibrierung FRC | Noch kein Firmware-/Downlink-Ablauf implementiert |
| Werksreset des SCD41 beim Boot | Wird nicht ausgeführt |

Die zwei Messungen waren schon vor dem Langzeit-Fix vorgesehen. Der neue Phasenschutz verhindert zusätzlich, dass nach Fehlern die falsche Messung als Nutzwert verwendet wird. Die etwa fünf Sekunden pro Shot sind eine Mess-/Stabilisierungszeit, **keine CO2-Referenzkalibrierung**. Die erste CO2-Nachricht nach einem Neustart kommt erst im vorgesehenen CO2-Zyklus; nicht zwingend zwölf Sekunden nach Einschalten.

Sensirion fordert im Power-cycled-Single-Shot-Modus das Verwerfen der ersten Messung und unterstützt in diesem Modus **keine ASC**. Deshalb wäre nur `SCD41_ASC_ENABLED=1` keine saubere Lösung für automatische Langzeitkalibrierung. [SCD4x Low Power Operation, Abschnitt 2.4](https://sensirion.com/media/documents/077BC86F/62BF01B9/CD_AN_SCD4x_Low_Power_Operation_D1.pdf)

### Passt der Sensor nach einem Neustart wieder?

**Ein gewöhnlicher MCU-Neustart setzt die Kalibrierung nicht absichtlich zurück und erfordert nicht allein deshalb eine neue Referenzkalibrierung.** Init weckt/stoppt/konfiguriert den Sensor, setzt aber keinen Werksreset ab. Danach wird wieder die erste Single-Shot-Messung verworfen. Bei einem vollständigen Akku-Abziehen startet zusätzlich der Sensor neu; die Initialisierung und das Doppel-Shot-Verfahren sind dafür vorgesehen.

Werkseinstellungen, gespeicherte Feldkalibrierung und flüchtige Konfiguration sind zu unterscheiden. Sensirion beschreibt die Konfigurationspersistenz über `persist_settings` und separat gespeicherte Feldkalibrierhistorie; die aktuelle Spezifikation nennt dafür Betriebsbedingungen. Aus `PERSIST_SETTINGS=0` folgt weder „Werkskalibrierung gelöscht“ noch „jede irgendwann begonnene Selbstkalibrierung sicher gespeichert“. [SCD4x-Datenblatt, Abschnitt 3.10.1](https://sensirion.com/media/documents/48C4B7FB/67FE0194/CD_DS_SCD4x_Datasheet_D1.pdf)

Was nicht geprüft ist: die tatsächlich gespeicherten Parameter deines physischen Sensors und seine absolute CO2-Abweichung gegenüber einer Referenz. Ein Neustart beseitigt eine vorhandene Kalibrierabweichung nicht. Auch ein vom Treiber als gültig gelesener CO2-Wert kann driften.

### Was für langfristig genaue CO2-Werte noch sinnvoll ist

Eine Referenzprüfung im endgültigen Gehäuse durchführen. Falls eine Korrektur erforderlich ist, braucht es einen geführten FRC-Wartungsablauf mit bekanntem, stabilem CO2-Referenzwert. Die Low-Power-Appnote beschreibt für den hier relevanten Betrieb eine Vorbereitung über fünf Minuten mit Messungen im Minutentakt; Versorgung und Umgebung müssen zur Anwendung passen. Nicht ungeprüft beim Boot auf pauschal 400 ppm setzen. [Sensirion, Abschnitt 5.1](https://sensirion.com/media/documents/077BC86F/62BF01B9/CD_AN_SCD4x_Low_Power_Operation_D1.pdf)

Für eine Implementierung müssten außerdem die spezifische FRC-Befehlsfolge, Fehlerantwort, Ergebnisprüfung, nachvollziehbare Referenz und Wiederanlauf im Normalmodus ergänzt werden. Aktuell gibt es **keinen** entsprechenden Downlink. `0x11` ist Lüfterreinigung, `0xFF` ein MCU-Reset; beide kalibrieren CO2 nicht.

Druckkompensation ist ein eigener Genauigkeitspunkt. Im Code wird der vorhandene BMP390-Wert nicht an SCD41 weitergegeben. Eine spätere Firmware-Erweiterung kann vor der CO2-Messung den gültigen aktuellen **absoluten Stationsdruck** verwenden; einen auf Meereshöhe umgerechneten Wetterdruck darf sie dafür nicht verwenden. Sensirion beschreibt Druck-/Höhenkompensation separat von FRC. [SCD4x-Datenblatt, Abschnitt 3.7](https://sensirion.com/media/documents/48C4B7FB/67FE0194/CD_DS_SCD4x_Datasheet_D1.pdf)

`SCD41_TEMPERATURE_OFFSET_C_X100=0` bedeutet im vorliegenden Treiber „keinen Offsetbefehl senden“, nicht „Sensoroffset sicher auf 0 °C“. Entsprechendes gilt für die Höhen-/Druck-Nullkonfiguration: vorhandene Sensorzustände werden nicht ausgelesen oder auf einen behaupteten Referenzwert geprüft. Der SCD41-T/RH-Offset ersetzt keine CO2-Referenzkalibrierung; seine Temperatur/Feuchte werden ohnehin nicht als Stationstemperatur/-feuchte gesendet.

## 11. Andere Sensoren und bereits angewandte Korrekturen

| Sensor | Aktueller Umgang | Nach Neustart / Backend-Konsequenz |
|---|---|---|
| SHT45 | Werkseitig kalibriert; Firmware liest Single-Shots und prüft CRC | Keine Neukalibrierung bei jedem Boot. Gehäuseeinfluss, Kondensation/Verschmutzung oder Langzeitdrift sind durch plausiblen Empfang nicht ausgeschlossen. |
| BMP390 | Firmware liest die sensorindividuellen NVM-Koeffizienten nach Initialisierung und berechnet kompensierten Druck | Die Koeffizienten werden erneut geladen. Gesendet wird Stationsdruck, nicht automatisch Meeresspiegeldruck; eine optionale Backend-Umrechnung getrennt kennzeichnen. |
| SPS30 | Digital kalibrierte Ausgabe; Firmware schaltet Messung/Sleep und löst Lüfterreinigung aus | Kein CO2-artiger Nullpunkt-Lernvorgang im Firmware-Boot. Reinigung ist Wartung, keine Neukalibrierung. Anlauf/Strömung/Verschmutzung bleiben relevant. |
| LTR390 | Firmware setzt Gain ×3/16 Bit und berechnet UVI; **Fensterfaktor 17/13 ist bereits eingerechnet** | Parameter werden neu gesetzt; Faktor steckt in der Firmware und bleibt erhalten. Backend teilt nur noch durch 100. Nach anderem Fenster/Alterung Referenzvergleich nötig, Faktor nicht doppelt anwenden. |
| MAX17048 | Treiber liest Zellspannung und prüft einen plausiblen Bereich | Gesendet werden mV, kein Ladezustand in %. Der aktuelle Treiber führt keine benutzerdefinierte SoC-/Quick-Start-Kalibrierung durch. |

Herstellerbelege: [SHT4x-Datenblatt](https://sensirion.com/resource/datasheet/sht4x), [BMP390-Datenblatt](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp390-ds002.pdf), [SPS30-Datenblatt](https://sensirion.com/media/documents/8600FF88/64A3B8D6/Sensirion_PM_Sensors_Datasheet_SPS30.pdf). Die konkreten Firmware-Korrekturen stehen in den zugehörigen Dateien unter `stm32_node/Core/Src/`.

Backendseitige spätere Kalibrierungsdaten sollten separat mit Station/Sensor, Verfahren, Referenz, Zeitpunkt, Firmware-/Konfigurationsstand und Gültigkeitsbeginn geführt werden. Nicht aus einem Bootzähler oder Sensor-valid-Bit einen Status „frisch kalibriert“ ableiten. Sensoreigene Kalibrierung und zusätzliche Backend-Korrektur dürfen nicht unbemerkt doppelt wirken.

## 12. Status für die Umsetzung

| Bereits in diesem Repository | Noch auszuführen |
|---|---|
| Firmware-Sentinels, feste Feldlayouts, optionale Diagnosen | Tatsächlichen HydroNode-Webhook-Decoder und nachgelagerte Qualitätsverarbeitung umbauen |
| Referenzdecoder und lokale Tests | End-to-End-Test durch TTN-Webhooks bis Datenbank/UI |
| CO2-Doppel-Shot und Phasenprüfung | Physische CO2-Referenzprüfung; FRC-/Druckkompensationsfunktion bei Bedarf ergänzen |
| Neustart lädt/initialisiert Sensoren, kein SCD41-Werksreset | Tatsächlich gespeicherte Sensorparameter bei Wartung auslesen/prüfen |
| Dokumentation der Hardware-/Wintergrenzen | Feldvalidierung und erst danach Flash-Rollout |

Quellbezug: [Sendelogik](../stm32_node/LoRaWAN/App/lora_app.c), [Messwerte/Gültigkeit](../stm32_node/Core/Src/sys_sensors.c), [Sensorkonfiguration](../stm32_node/Core/Inc/sys_conf.h), [SCD41-Treiber](../stm32_node/Core/Src/scd41.c), [Diagnosezustand](../stm32_node/Core/Src/runtime_health.c), [Referenzdecoder](payload-decoder.js), [Reliability-/Flash-Hinweise](RELIABILITY.md). Diese Markdown ist die Übergabe, kein Nachweis eines bereits angepassten produktiven Backends.
