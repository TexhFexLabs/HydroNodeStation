# HydroNodeStation01

Eine autarke, hocheffiziente Wetterstation, konzipiert für das HydroNode-Netzwerk. Das Projekt demonstriert, wie mit minimalem Energiebedarf, einer kleinen Batterie und einem kompakten Solarpanel eine Vielzahl präziser Umweltdaten erfasst und per LoRaWAN übermittelt werden kann. Ziel ist der flächendeckende Einsatz (z.B. im Raum Regensburg), um öffentlich zugängliche Echtzeit-Wetterdaten und weitreichende Analysen bereitzustellen.

## Systemarchitektur

Das System ist in drei Hauptbereiche unterteilt: Hardware, Software (STM32 Node) und Mechanik (CAD).

### Hardware & Sensoren
Die Elektronik ist auf maximale Effizienz und Kompaktheit ausgelegt. Das Custom-PCB bündelt fortschrittliche Sensorik und ein intelligentes Power-Management:
* **Mikrocontroller & Funk:** STM32WLE5 (integriertes LoRaWAN)
* **Power Management:** TPS63900 (Buck-Boost) und MAX17048 (Batterie-IC) für Solar- und Batteriebetrieb
* **Klima & Luftgüte:** Sensirion SCD41 (CO2), SPS30 (Feinstaub), SHT45 (Temperatur/Luftfeuchtigkeit)
* **Wetter & Licht:** Bosch BMP390 (Luftdruck), LTR390 (UV/Umgebungslicht)

*[Platzhalter: Render des finalen PCBs von JLCPCB]*

### Software (STM32 Node)
Die Firmware basiert auf dem STM32Cube-Ökosystem und implementiert einen LoRaWAN End-Node. Der Fokus der Entwicklung liegt auf tiefgreifender Energieoptimierung:
* Nutzung des STOP2-Modus für minimalen Ruhestrom
* Effizientes Sensor-Auslesen durch präzises Timing
* Übertragung komprimierter Payloads über das LoRa-Netzwerk

### Mechanik (CAD)
Die Elektronik wird in einem speziell entwickelten Gehäuse (Stevenson Screen) untergebracht. Dieses schützt die Komponenten vor Witterungseinflüssen, während es gleichzeitig eine optimale Luftzirkulation für exakte Messwerte gewährleistet.

*[Platzhalter: CAD-Rendering des fertigen Gehäuses]*

## Projektphasen

Das Projekt durchläuft vier klar definierte Phasen. Derzeit befinden wir uns am Übergang zur Fertigung und Gehäuseentwicklung.

**1. Hardware & Software (Aktuell)**
Die Hardware-Entwicklung ist abgeschlossen. Das finale PCB wird derzeit bei JLCPCB gefertigt und bestückt. Parallel wird die Firmware auf den neuen STM32-Chip portiert und finalisiert.

**2. Gehäuse-Design (Demnächst)**
Sobald die Hardware vorliegt, startet die CAD-Konstruktion für ein kompaktes und ästhetisches Gehäuse.

**3. Feldtests (Geplant)**
Die fertigen Prototypen werden an verschiedenen Standorten installiert. Hier werden Zuverlässigkeit, Energieverbrauch und Sensorpräzision unter realen Bedingungen validiert.

**4. HydroNode Integration (Geplant)**
Die Stationen werden vollständig in das HydroNode-Netzwerk integriert. Die gesammelten Daten fließen in ein Backend zur tiefgehenden Analyse und werden für Endnutzer visuell aufbereitet.

## System-Integration

```text
[ Sensor Node ] ~LoRaWAN~> [ Gateway ] ==> [ HydroNode Backend ] ==> [ Dashboard & Analytics ]
```
*[Platzhalter: Grafische Visualisierung der Netzwerk-Architektur und Datenanalyse]*