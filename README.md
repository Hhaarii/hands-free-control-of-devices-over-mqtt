# Hands-Free Control of Devices & ESP32 Telemetry Over MQTT

Production-grade cross-platform device control hub, ESP32 live telemetry streaming over MQTT, SQLite database logger, and real-time Chart.js Web UI analytics dashboard.

---

## 🌟 Key Features

1. **ESP32 Microcontroller Live Telemetry:**
   - C++/Arduino firmware ([esp32_telemetry.ino](file:///home/ha-r1/hub-ui/esp32_telemetry.ino)) publishing JSON sensor data over MQTT (`telemetry/esp32_node1/data`).
   - Telemetry metrics: Temperature (°C), Humidity (%), Supply Voltage (V), Wi-Fi RSSI (dBm), Free Heap Memory, and Uptime.
   - Remote command listener for on-board LED toggle (`led on` / `led off`) and ping checks.

2. **SQLite Database Logger Daemon:**
   - Background Python daemon ([telemetry_logger.py](file:///home/ha-r1/hub-ui/telemetry_logger.py)) subscribing to `telemetry/+/data`.
   - Persists all telemetry metrics into `telemetry.db`.
   - Answers historical database queries over MQTT (`telemetry/query` -> `telemetry/history/response`), keeping data exchange 100% inside MQTT.

3. **Web UI Analytics Dashboard:**
   - Dark glassmorphism dashboard ([index.html](file:///home/ha-r1/hub-ui/index.html)) with WebSockets (`mqtt.min.js`) and **Chart.js**.
   - 6 Live Metric Status Badges (Temp, Hum, Volt, RSSI, Heap, Uptime).
   - 3 Interactive Real-Time & Historical Charts:
     - Temperature (°C) & Humidity (%) Dual-Axis Line Chart.
     - Wi-Fi Signal RSSI (dBm) & Supply Voltage (V) Line Chart.
     - Free Heap Memory (KB) Chart.
   - "Fetch SQLite History" button to render past historical trends from the database.

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

### 3. Flash ESP32 Microcontroller
1. Open `esp32_telemetry.ino` in Arduino IDE or VS Code PlatformIO.
2. Install libraries: `PubSubClient` and `ArduinoJson`.
3. Update `WIFI_SSID`, `WIFI_PASSWORD`, and `MQTT_SERVER` (`192.168.1.82`).
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
