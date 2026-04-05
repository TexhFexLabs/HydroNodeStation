/*
 * HydroNode Sensor Station
 * ========================
 * Liest folgende Sensoren aus:
 *   - MS8607:    Temperatur & Luftfeuchtigkeit (Adafruit MS8607 Library)
 *   - LC709203F: Batterie-Spannung (Adafruit LC709203F Library)
 *   - VEML6075:  UVI, UVA, UVB (Adafruit VEML6075 Library)
 *
 * Versendet alle Werte über die HydroNode Library (TechFexLabs)
 * an das HydroNode IoT Backend.
 *
 * Deep Sleep Modus: ESP wacht alle 2 Minuten auf, misst und sendet,
 * dann schläft er wieder. Kein loop() nötig.
 *
 * Benötigte Libraries (Arduino Library Manager):
 *   - Adafruit MS8607          (+ Adafruit BusIO, Adafruit Unified Sensor)
 *   - Adafruit LC709203F       (+ Adafruit BusIO)
 *   - Adafruit VEML6075        (+ Adafruit BusIO)
 *   - HydroNode-Library        (https://github.com/TexhFexLabs/HydroNode-Library)
 *   - WiFiManager, ArduinoJson, ArduinoHttpClient, NTPClient,
 *     densaugeo/ArduinoBase64, Crypto (HydroNode Deps)
 *
 * WICHTIG (Hardware):
 *   Für Deep Sleep auf ESP32 muss GPIO16 (RST) mit EN verbunden sein,
 *   damit der Timer-Wakeup funktioniert!
 */

#include <Wire.h>
#include <WiFiManager.h>

// --- Sensor Libraries (Adafruit) ---
#include <Adafruit_MS8607.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_LC709203F.h"
#include "Adafruit_VEML6075.h"

// --- HydroNode Library (TechFexLabs / Felix Knoll) ---
#include <HydroNode.h>

// =====================================================================
// KONFIGURATION
// =====================================================================
#define HYDRONODE_SENSOR_ID   "e0fae10a-8406-43be-bb2a-ac03756a7384"
#define HYDRONODE_SECRET      "5XJUdH0dpFoZJ5zhI6QkRsRjqNIo1pvE"

// Deep Sleep Intervall: 2 Minuten in Mikrosekunden
#define SLEEP_DURATION_US     (5ULL * 60ULL * 1000000ULL)

// LC709203F Akku-Größe
#define BATTERY_PACK_SIZE     LC709203F_APA_1000MAH

// =====================================================================
// Globale Objekte
// =====================================================================
Adafruit_MS8607     ms8607;
Adafruit_LC709203F  lc;
Adafruit_VEML6075   uv = Adafruit_VEML6075();

HydroNode hydro(HYDRONODE_SENSOR_ID, HYDRONODE_SECRET);

// =====================================================================
// SETUP – läuft bei jedem Wakeup komplett neu durch
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n========================================");
  Serial.println("  HydroNode Sensor Station (Deep Sleep)");
  Serial.println("========================================\n");

  // Wakeup-Grund ausgeben (Info)
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Wakeup: Timer-Interrupt");
  } else {
    Serial.println("Wakeup: Erster Start / Reset");
  }

  Wire.begin();

  // --- WiFi via WiFiManager ---
  WiFiManager wm;
  // Timeout für den AP: 180 Sekunden, dann Neustart
  // (verhindert, dass der ESP ewig im AP-Modus hängt)
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(30);       // 30 Sekunden auf Verbindung warten
  wm.setConnectRetries(3); 

  String apName = hydro.getApName();
  Serial.print("Verbinde WiFi (AP-Fallback: ");
  Serial.print(apName);
  Serial.println(")");

  bool wifiConnected = wm.autoConnect(apName.c_str());
  if (!wifiConnected) {
    Serial.println("WiFi-Verbindung fehlgeschlagen! Schlafe 2 min und versuche erneut...");
    goToSleep();
    return; // Wird nicht erreicht, aber für Klarheit
  }
  Serial.println("WiFi verbunden!");

  // --- HydroNode initialisieren ---
  hydro.begin();
  Serial.println("HydroNode initialisiert.\n");

  // --- Sensoren initialisieren und Daten senden ---
  readAndSendSensors();

  // --- Deep Sleep einleiten ---
  Serial.println("\n--- Gehe in Deep Sleep für 2 Minuten ---\n");
  goToSleep();
}

// =====================================================================
// LOOP – wird nie erreicht, da setup() mit goToSleep() endet
// =====================================================================
void loop() {
  // Intentionally empty – der ESP schläft, bevor loop() je läuft.
}

// =====================================================================
// Sensoren auslesen und Werte senden
// =====================================================================
void readAndSendSensors() {
  Serial.println("--- Sensor-Werte auslesen ---");

  // -----------------------------------------------------------------
  // MS8607: Temperatur & Luftfeuchtigkeit
  // -----------------------------------------------------------------
  if (ms8607.begin()) {
    ms8607.setHumidityResolution(MS8607_HUMIDITY_RESOLUTION_OSR_12b);

    sensors_event_t temp, pressure, humidity;
    ms8607.getEvent(&pressure, &temp, &humidity);

    float temperature = temp.temperature;
    float humid       = humidity.relative_humidity;

    Serial.print("  Temperatur:       ");
    Serial.print(temperature, 2);
    Serial.println(" °C");

    Serial.print("  Luftfeuchtigkeit: ");
    Serial.print(humid, 2);
    Serial.println(" %");

    hydro.sendValue("temperature", temperature);
    hydro.sendValue("humidity", humid);
  } else {
    Serial.println("  MS8607: nicht gefunden, übersprungen.");
  }

  // -----------------------------------------------------------------
  // LC709203F: Batterie-Spannung
  // -----------------------------------------------------------------
  if (lc.begin()) {
    lc.setThermistorB(3950);
    lc.setPackSize(BATTERY_PACK_SIZE);
    lc.setAlarmVoltage(3.8);

    float battVoltage = lc.cellVoltage();
    float battPercent = lc.cellPercent();

    Serial.print("  Batt Voltage:     ");
    Serial.print(battVoltage, 3);
    Serial.println(" V");

    Serial.print("  Batt Percent:     ");
    Serial.print(battPercent, 1);
    Serial.println(" %");

    hydro.sendValue("battery_voltage", battVoltage);
    hydro.sendValue("battery_percent", battPercent);
  } else {
    Serial.println("  LC709203F: nicht gefunden, übersprungen.");
  }

  // -----------------------------------------------------------------
  // VEML6075: UV-Sensor
  // -----------------------------------------------------------------
  if (uv.begin()) {
    float uvIndex = uv.readUVI();
    float uva     = uv.readUVA();
    float uvb     = uv.readUVB();

    Serial.print("  UV Index:         ");
    Serial.println(uvIndex, 2);

    Serial.print("  UVA:              ");
    Serial.println(uva, 2);

    Serial.print("  UVB:              ");
    Serial.println(uvb, 2);

    hydro.sendValue("uv_index", uvIndex);
    hydro.sendValue("uva", uva);
    hydro.sendValue("uvb", uvb);
  } else {
    Serial.println("  VEML6075: nicht gefunden, übersprungen.");
  }

  Serial.println("--- Senden abgeschlossen ---");
}

// =====================================================================
// Deep Sleep einleiten
// =====================================================================
void goToSleep() {
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
  Serial.flush(); // Sicherstellen, dass Serial-Output vollständig ist
  esp_deep_sleep_start();
}