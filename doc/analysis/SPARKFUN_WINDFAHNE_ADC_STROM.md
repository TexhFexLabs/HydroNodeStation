# SparkFun-Windfahne: ADC-Beschaltung und Strom

Stand: 06.10.2026. Forschung/Planung für zweite Station, keine Firmwareänderung. Basis: SparkFun Weather Meter SEN-15901; passende Kitrevision/Steckerbelegung prüfen.

## Anschluss

Die Windfahne ist passiv: acht Reedkontakte und Widerstände erzeugen 16 mögliche Widerstandswerte, teils durch zwei gleichzeitig geschlossene Kontakte. Sie hat keine eigene I2C-Schnittstelle und gibt ohne externe Speisung keine auslesbare Spannung aus.

Benötigt werden **ein ADC-Eingang** und ein externer **10-kΩ-Widerstand, vorzugsweise 1 %**. Für niedrigen mittleren Verbrauch den Widerstand über einen GPIO speisen:

`GPIO-Ausgang → 10 kΩ → Messpunkt → Windfahne → GND`

Der Messpunkt geht zusätzlich zum ADC-Eingang. GPIO HIGH entspricht etwa 3,3 V, GPIO LOW schaltet den Teiler praktisch stromlos. Die zwei Windfahnenleitungen liegen zwischen Messpunkt und GND. Ein dauerhaftes 3,3-V-Netz könnte ebenfalls speisen, würde jedoch den Teilerstrom ständig ziehen.

Der eine freie STM32-Analogeingang reicht dafür. Optional ein externer I2C-ADC wie ADS1115, falls der Pin anderweitig verwendet wird. Die fertigen Regen-/Windgeschwindigkeits-Impulszähler PA4/PA5 zählen Kontakte; sie ersetzen keine Widerstandsauswertung der Windrichtung.

## Spannung und Ströme bei 3,3 V

`V_ADC = 3,3 V × R_Fahne / (10 kΩ + R_Fahne)`

`I_Teiler = 3,3 V / (10 kΩ + R_Fahne)`

| Ausrichtung | Widerstand | Nominelle ADC-Spannung | Strom während Speisung |
|---|---:|---:|---:|
| 0° / N | 33 kΩ | 2,533 V | 76,7 µA |
| 90° / E | 1 kΩ | 0,300 V | 300,0 µA |
| 112,5° / ESE | 688 Ω | 0,212 V | 308,8 µA |
| 180° / S | 3,9 kΩ | 0,926 V | 237,4 µA |
| 270° / W | 120 kΩ | 3,046 V | 25,4 µA |

Die vollständige 16-Richtungen-Widerstandstabelle steht im Datenblatt. Richtungszuordnung anhand kalibrierter Spannungsbereiche, nicht nur exakter Sollwerte; Widerstände, GPIO-Voh, Kontakt- und Leitungseffekte erzeugen Abweichungen. Sensor mechanisch nach Norden ausrichten. ADC-Referenz/Vdda berücksichtigen, möglichst ratiometrisch messen. Die Datenblattbeispielschaltung mit 5 V nicht ungeprüft an 3,3-V-ADC anschließen.

Die GPIO-Speisung ist bei höchstens etwa 0,31 mA eine kleine Last; konkrete GPIO-Voh und ADC-Werte am Board prüfen. Dessen interne Pull-up-Toleranz ist kein Ersatz für einen definierten externen 10-kΩ-Widerstand.

## Stromsparen und Einlesen

1. GPIO auf HIGH setzen.
2. Spannung einschwingen lassen und mehrere ADC-Werte lesen/mitteln.
3. GPIO auf LOW, ADC nach Messsitzung abschalten.

Optionaler RC-Filter zur Störunterdrückung: beispielhaft 1 kΩ Serie zum ADC und 10 nF gegen GND an dessen Eingang. Worst-case Quellwiderstand des Teilers ca. 9,23 kΩ; mit 1 kΩ Serie und 10 nF liegt die RC-Zeitkonstante bei ca. 0,10 ms. Etwa 1–2 ms Wartezeit ist dafür ein plausibler Ausgangspunkt, nicht eine Messzeitgarantie für beliebige Kabel/Filter/ADC-Einstellungen. ADC-Abtastzeit nach Eingangsimpedanz einstellen. Größere Kondensatoren verlängern die Wartezeit.

Für **2 ms aktiv pro Sekunde** ergibt sich rechnerisch:

- Während eingeschaltet: etwa **25–309 µA** aus 3,3 V.
- Während aus: **praktisch kein Teilerstrom**, reale GPIO-/Schaltungsleckströme bleiben.
- Durchschnitt: etwa **0,051–0,618 µA**.

Bei 2 ms alle 60 s: etwa **0,00085–0,0103 µA** mittlerer Teilerstrom. Das Beispiel betrachtet nur den passiven Teiler. **ADC, MCU-Wachzeit, Kommunikation und Schutz-/Zusatzschaltung fehlen darin.** Diese können das Windfahnenbudget übersteigen; nicht als Gesamtstationsstrom ausgeben. Beim externen ADC außerdem Wandlungszeit und Ruhestrom berücksichtigen, statt pauschal die 2-ms-MCU-Messung anzunehmen.

1 Hz ist ein praktikabler Ausgangspunkt für Richtungssampling; Übertragung kann seltener erfolgen. Winkel bei späterer Mittelung zirkulär behandeln, z. B. 359°/1° nicht arithmetisch zu 180° mitteln. Bei sehr schwachem Wind ist die mechanische Ausrichtung weniger aussagekräftig.

## Quellen und Prüfung

- [SparkFun Weather Meter Hookup Guide](https://learn.sparkfun.com/tutorials/weather-meter-hookup-guide/all): passive Messgeber, Kontakte und Spannungsteiler.
- [SEN-15901 Datenblatt](https://cdn.sparkfun.com/assets/d/1/e/0/6/DS-15901-Weather_Meter.pdf): 16 Widerstände, Minimum 688 Ω / Maximum 120 kΩ. Die relevante Seite wurde gerendert und visuell geprüft.
- [SparkFun Weather Meter Kit](https://www.sparkfun.com/weather-meter-kit.html): konkrete Kitrevision.
- Bestehender Projektcode: ADC-Solarpfad PB2 bereits belegt; weitere freie Anschlüsse/Pinbelegung für Windfahne erst im konkreten Aufbau festlegen.

Nominelle Werte durchgerechnet; kein echter Sensor vermessen und kein Pin/Treiber implementiert.
