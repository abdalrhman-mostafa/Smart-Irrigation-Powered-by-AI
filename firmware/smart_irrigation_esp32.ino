/**
 * Smart Irrigation & Water Quality Monitoring System
 * 
 * Microcontroller: ESP32
 * 
 * Hardware Pinout:
 *   - Soil / Water Level Sensor: GPIO35 (Analog ADC1)
 *   - TDS Water Quality Sensor:  GPIO33 (Analog ADC1)
 *   - DHT22 Temp & Humidity:     GPIO4  (Digital)
 *   - Relay Module (DC Pump):    GPIO25 (Digital Output, Active-LOW)
 * 
 * Cloud Platforms:
 *   - Blynk IoT (Mobile App)
 *   - Firebase Realtime Database (Web App & Python AI Engine)
 */

#define BLYNK_TEMPLATE_ID   "TMPL2lLcpegAV"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Powered by AI"
#define BLYNK_AUTH_TOKEN    "-FsNnoRbxEY9ILoPpcR3d8LD_ZErmLXe"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "DHTesp.h"

// 1. WiFi & Firebase Configuration
const char WIFI_SSID[] = "Gemy";
const char WIFI_PASS[] = "P@ssw0rd";

#define FIREBASE_HOST   "https://abdalrhman-mostafa-default-rtdb.firebaseio.com/"
#define FIREBASE_SECRET "jwaSG9CYvRvf1mgPjRdK4kGyCYHEDrfkh07aam8b"

// 2. Hardware Pin Definitions
#define SOIL_PIN        35   // Water/Soil Sensor (ADC1)
#define TDS_PIN         33   // TDS Water Quality Sensor (ADC1)
#define DHT_PIN         4    // DHT22 Temperature & Humidity Sensor
#define RELAY_PIN       25   // Relay Module Control (Active-LOW)

// 3. Calibration & Thresholds
// For Resistive Water Sensor: 0 = completely dry, ~2500 = fully wet
const int DRY_SENSOR_RAW = 50;      
const int WET_SENSOR_RAW = 2500;    
const int DRY_THRESHOLD = 30;       // Below 30% -> Turn pump ON
const int WET_THRESHOLD = 65;       // Above 65% -> Turn pump OFF
const float MAX_SAFE_TDS = 1200.0;  // Max safe salinity (ppm)

// System State Variables
int moisturePercent = 0;
float currentTemp = 0.0;
float currentHum = 0.0;
float currentTDS = 0.0;
bool relayState = false;
bool manualMode = false;
bool autoMode = true;

unsigned long pumpStartTime = 0;
const unsigned long MAX_PUMP_RUN_MS = 25000; // 25s safety timeout

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
DHTesp dht;
BlynkTimer timer;

// Function: Control Relay & Pump (Active-LOW)
void setPump(bool turnOn) {
  // Salinity Safety Interlock: Block pump if water is hazardous
  if (turnOn && currentTDS > MAX_SAFE_TDS) {
    Serial.println("[SAFETY INTERLOCK] Pump blocked! Water salinity too high (>1200 ppm)!");
    Blynk.logEvent("salinity_alert", "Water salinity hazardous! Irrigation blocked.");
    return;
  }

  relayState = turnOn;
  digitalWrite(RELAY_PIN, turnOn ? LOW : HIGH); // Active-LOW: LOW = ON, HIGH = OFF
  
  if (turnOn) {
    pumpStartTime = millis();
    Serial.println("[PUMP] Activated -> Water Flowing 💧");
  } else {
    Serial.println("[PUMP] Deactivated -> Stopped ⏹️");
  }

  // Update cloud status
  Blynk.virtualWrite(V2, turnOn ? 1 : 0);
  Firebase.RTDB.setBool(&fbdo, "/live/pumpActive", turnOn);
}

// Function: Read Analog TDS Sensor (ppm)
float readTDSValue() {
  int raw = analogRead(TDS_PIN);
  float voltage = raw * (3.3 / 4095.0);
  float compensationCoefficient = 1.0;
  float compensationVoltage = voltage / compensationCoefficient;
  float tds = (133.42 * pow(compensationVoltage, 3) 
             - 255.86 * pow(compensationVoltage, 2) 
             + 857.39 * compensationVoltage) * 0.5;
  return max(0.0f, tds);
}

// Function: Read All Sensors & Handle Auto Irrigation
void checkSensors() {
  // 1. Water / Soil Moisture Sensor
  int rawSoil = analogRead(SOIL_PIN);
  // Resistive sensor reading increases as water touches it
  moisturePercent = map(rawSoil, DRY_SENSOR_RAW, WET_SENSOR_RAW, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // 2. Temperature & Humidity from DHT22
  TempAndHumidity dhtData = dht.getTempAndHumidity();
  if (!isnan(dhtData.temperature)) currentTemp = dhtData.temperature;
  if (!isnan(dhtData.humidity)) currentHum = dhtData.humidity;

  // 3. Water Quality (TDS)
  currentTDS = readTDSValue();

  Serial.printf("[Telemetry] Soil: %d%% (raw:%d) | Temp: %.1fC | Hum: %.1f%% | TDS: %.1f ppm\n", 
                moisturePercent, rawSoil, currentTemp, currentHum, currentTDS);

  // Send Telemetry to Blynk
  Blynk.virtualWrite(V1, moisturePercent);
  Blynk.virtualWrite(V5, currentTDS);

  // Automatic Irrigation Logic
  if (autoMode && !manualMode) {
    if (moisturePercent < DRY_THRESHOLD && !relayState) {
      Serial.println("[AUTO] Soil is dry. Turning pump ON...");
      setPump(true);
    } else if (moisturePercent >= WET_THRESHOLD && relayState) {
      Serial.println("[AUTO] Sufficient moisture reached. Turning pump OFF.");
      setPump(false);
    }
  }

  // Safety Timeout Check
  if (relayState && (millis() - pumpStartTime >= MAX_PUMP_RUN_MS)) {
    Serial.println("[SAFETY] Pump exceeded maximum runtime limit. Forcing OFF.");
    setPump(false);
  }
}

// Function: Push Telemetry to Firebase
void sendToFirebase() {
  if (!Firebase.ready()) return;

  // Live status
  FirebaseJson live;
  live.set("soilMoisture", moisturePercent);
  live.set("temperature", currentTemp);
  live.set("humidity", currentHum);
  live.set("tds", currentTDS);
  live.set("pumpActive", relayState);
  live.set("lastUpdated", (int)(millis() / 1000));
  Firebase.RTDB.setJSON(&fbdo, "/live", &live);

  // History log for Python AI Engine
  String path = "/readings/" + String(millis());
  FirebaseJson reading;
  reading.set("moisture", moisturePercent);
  reading.set("tds", currentTDS);
  reading.set("relay", relayState ? "ON" : "OFF");
  reading.set("mode", manualMode ? "Manual" : "Auto");
  reading.set("timestamp", millis());
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &reading);
}

// Function: Read AI Predictions from Firebase
void readAIPredictions() {
  if (!Firebase.ready()) return;

  FirebaseData predData;
  if (Firebase.RTDB.getJSON(&predData, "/predictions")) {
    FirebaseJson &json = predData.jsonObject();
    FirebaseJsonData result;

    json.get(result, "next_moisture");
    Blynk.virtualWrite(V3, result.floatValue);

    json.get(result, "hours_until_dry");
    Blynk.virtualWrite(V4, result.floatValue);
  }
}

// Blynk: Manual Switch (V0)
BLYNK_WRITE(V0) {
  int val = param.asInt();
  manualMode = (val == 1);
  setPump(val == 1);
}

void setup() {
  Serial.begin(115200);

  // Relay Setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Active-LOW: Start OFF

  // Sensors Setup
  dht.setup(DHT_PIN, DHTesp::DHT22);
  analogReadResolution(12);

  // Cloud Connections
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Timers
  timer.setInterval(3000L, checkSensors);       // Every 3 sec
  timer.setInterval(60000L, sendToFirebase);    // Every 1 min
  timer.setInterval(30000L, readAIPredictions); // Every 30 sec

  Serial.println("[System Ready] Smart Irrigation & TDS Quality System Online!");
}

void loop() {
  Blynk.run();
  timer.run();
}
