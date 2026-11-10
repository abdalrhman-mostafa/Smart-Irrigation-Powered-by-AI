# Smart Irrigation & Water Quality System (ESP32 + AI + Cloud)

An intelligent, multi-sensor IoT irrigation system with AI predictive modeling, real-time soil moisture monitoring, and TDS water quality safety interlock.

---

## 📁 Project Structure

```text
smart-irrigation-system/
│
├── firmware/
│   └── smart_irrigation_esp32.ino   # ESP32 C++ firmware (Blynk + Firebase + Sensors)
│
├── ai-engine/
│   ├── ai_model.py                  # Random Forest Regressor & Anomaly Detection engine
│   └── requirements.txt             # Python dependencies
│
├── web-app/
│   └── index.html                   # Interactive live web dashboard (PWA)
│
├── database/
│   ├── database.rules.json          # Firebase Realtime Database security rules
│   └── initial_data.json            # Initial database schema
│
├── .gitignore
└── README.md
```

---

## 🔌 Hardware Pinout Reference

| Component | Pin on Sensor | Connect to ESP32 Pin | Power | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Water / Soil Sensor** | `S` (Signal)<br>`+`<br>`-` | **GPIO 35**<br>**3.3V**<br>**GND** | 3.3V | ADC1 channel (WiFi safe). Dip parallel tracks into soil. |
| **TDS Water Quality Sensor** | `A` (Signal)<br>`+`<br>`-` | **GPIO 33**<br>**3.3V**<br>**GND** | 3.3V | ADC1 channel (WiFi safe). Submerge probe in water tank. |
| **DHT22 (Temp & Humidity)** | `DATA`<br>`VCC`<br>`GND` | **GPIO 4**<br>**3.3V**<br>**GND** | 3.3V | Digital single-bus ambient climate sensor. |
| **Relay Module (DC Pump)** | `IN1`<br>`VCC`<br>`GND` | **GPIO 25**<br>**5V (VIN)**<br>**GND** | 5V | Active-LOW control for water pump. |

---

## 🚀 Step-by-Step Execution Guide

### 1. Hardware Assembly
Wire your 4 components to the ESP32 according to the table above.

### 2. Arduino IDE (ESP32)
1. Install libraries in Arduino IDE (`Tools > Manage Libraries...`):
   - `Blynk` by Volodymyr Shymanskyy
   - `Firebase-ESP-Client` by Mobizt
   - `DHT sensor library for ESPx`
2. Open [`firmware/smart_irrigation_esp32.ino`](file:///C:/Users/bedom/.gemini/antigravity/scratch/smart-irrigation-system/firmware/smart_irrigation_esp32.ino).
3. Set your WiFi credentials on lines 31–32:
   ```cpp
   const char WIFI_SSID[] = "YOUR_WIFI_NAME";
   const char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";
   ```
4. Select `ESP32 Dev Module` and your COM port, then click **Upload**.

### 3. Python AI Engine
1. Open a terminal inside the `ai-engine/` folder:
   ```bash
   cd ai-engine
   pip install -r requirements.txt
   ```
2. Place your Firebase `serviceAccountKey.json` inside the `ai-engine/` folder.
3. Start the AI script:
   ```bash
   python ai_model.py
   ```

### 4. Web Dashboard & Blynk
* **Web Dashboard:** Open `web-app/index.html` in your browser.
* **Blynk App:** Open your phone app; it will show live moisture, TDS water quality, temperature, and AI predictions!
