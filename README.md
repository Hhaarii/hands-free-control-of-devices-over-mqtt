# Hands-Free Control of Devices & Real-Time ESP32 Sensor Telemetry Over MQTT

Production-grade cross-platform device control hub, DHT11 real-time sensor telemetry streaming over MQTT, SQLite database logger, and interactive Chart.js Web UI analytics dashboard.

---

## 🌟 Key Features

1. **DHT11 Real-Time Hardware Sensor Monitoring:**
   - C++/Arduino firmware ([esp32_telemetry.ino](file:///home/ha-r1/hub-ui/esp32_telemetry.ino)) reading live sensor metrics from **DHT11 on GPIO 15 (D15)**.
   - **Hardware Wiring:**
     - `VCC` $\rightarrow$ `3.3V` of ESP32
     - `GND` $\rightarrow$ `GND` of ESP32
     - `Data OUT` $\rightarrow$ `GPIO 15` (Pin D15 of ESP32)
   - Real-Time Metrics: Live Temperature (°C), Humidity (%), Supply Voltage (V), Wi-Fi RSSI (dBm), Free Heap Memory, and Uptime.
   - Remote command listener for on-board LED control (`led on` / `led off`) and ping checks.

2. **SQLite Database Logger Daemon:**
   - Background Python daemon ([telemetry_logger.py](file:///home/ha-r1/hub-ui/telemetry_logger.py)) subscribing to `telemetry/+/data`.
   - Persists all real-time sensor metrics into `telemetry.db` with exact timestamps.
   - Serves historical database queries over MQTT (`telemetry/query` -> `telemetry/history/response`), keeping data exchange 100% inside MQTT.

3. **Real-Time Web UI Analytics Dashboard:**
   - Dark glassmorphism dashboard ([index.html](file:///home/ha-r1/hub-ui/index.html)) with WebSockets (`mqtt.min.js`) and **Chart.js**.
   - 6 Live Metric Badges (Temp, Hum, Volt, RSSI, Heap, Uptime).
   - 3 Interactive Real-Time & Historical Charts:
     - **Temperature (°C) & Humidity (%) Dual-Axis Line Chart**
     - **Wi-Fi Signal RSSI (dBm) & Supply Voltage (V) Line Chart**
     - **Free Heap Memory (KB) Chart**
   - "Fetch SQLite History" button to render past historical sensor trends from the database.

4. **Smartphone & Laptop Remote Control:**
   - Remote screen mirror with Click-to-Tap gesture dispatch.
   - Remote typing, keypresses (Home, Back, Recents, Power), app launcher, media and volume control.

---

## 🚀 Quick Start Guide

### 1. Launch Mosquitto MQTT Broker
Ensure Mosquitto is running with WebSockets enabled on port 9001:
```bash
sudo systemctl restart mosquitto
```

### 2. Start SQLite Telemetry Logger Daemon
```bash
python3 telemetry_logger.py
```

### 3. Flash ESP32 Microcontroller (DHT11 on GPIO 15)
1. Open `esp32_telemetry.ino` in Arduino IDE.
2. Ensure libraries are installed: `PubSubClient`, `ArduinoJson`, `DHT sensor library`, and `Adafruit Unified Sensor`.
3. Update `WIFI_PASSWORD` and `MQTT_SERVER` (`192.168.1.119` / `localhost`).
4. Upload to ESP32 board.

### 4. (Optional) Run ESP32 Simulator for Testing
If you want to test the telemetry logger and UI graphs without physical hardware:
```bash
python3 test_esp32_sim.py
```

### 5. Open Web Dashboard
Open `index.html` in any web browser and click **🔌 Connect Broker**. Click **💾 Fetch SQLite History** to view stored historical graphs!

---

*Maintained and synchronized by offGIT harness.*
