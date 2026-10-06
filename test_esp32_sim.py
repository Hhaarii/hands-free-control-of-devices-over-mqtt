#!/usr/bin/env python3
"""
ESP32 Telemetry Simulator
Generates realistic simulated sensor telemetry for ESP32 and publishes to MQTT.
"""

import time
import json
import random
import paho.mqtt.client as mqtt

MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883
MQTT_USER = "lap1"
MQTT_PASS = "HARI@MQTT"
DEVICE_ID = "esp32_node1"

client = mqtt.Client(client_id="sim-esp32")
client.username_pw_set(MQTT_USER, MQTT_PASS)

print("Connecting simulator to Mosquitto Broker...")
client.connect(MQTT_HOST, MQTT_PORT, 60)
client.publish("devices/esp32_node1/status", "online", retain=True)

temp = 24.5
hum = 52.0
volt = 3.28
rssi = -64
heap = 215000
uptime = 0

print("Publishing simulated telemetry every 3 seconds... Press Ctrl+C to stop.")
try:
    while True:
        temp += random.uniform(-0.4, 0.4)
        hum += random.uniform(-0.8, 0.8)
        volt += random.uniform(-0.02, 0.02)
        rssi += random.randint(-2, 2)
        heap += random.randint(-500, 500)
        uptime += 3

        # Clamp bounds
        temp = max(18.0, min(38.0, round(temp, 1)))
        hum = max(30.0, min(85.0, round(hum, 1)))
        volt = max(3.0, min(3.3, round(volt, 2)))
        rssi = max(-85, min(-45, rssi))

        payload = {
            "device_id": DEVICE_ID,
            "temperature": temp,
            "humidity": hum,
            "voltage": volt,
            "rssi": rssi,
            "free_heap": heap,
            "uptime": uptime
        }

        json_str = json.dumps(payload)
        client.publish(f"telemetry/{DEVICE_ID}/data", json_str)
        print(f"[Simulated ESP32] Published: {json_str}")
        time.sleep(3)
except KeyboardInterrupt:
    print("\nSimulator stopped.")
    client.disconnect()
