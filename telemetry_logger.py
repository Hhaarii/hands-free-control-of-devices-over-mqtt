#!/usr/bin/env python3
"""
SQLite Telemetry Logger & Broker Query Service
Project: Hands-Free Device Control Over MQTT
Description: Subscribes to MQTT telemetry topics, persists JSON metrics to SQLite, 
             and serves historical log queries directly over MQTT.
"""

import os
import json
import sqlite3
import time
import signal
import sys
import paho.mqtt.client as mqtt

# Configuration defaults
MQTT_HOST = os.environ.get("MQTT_HOST", "127.0.0.1")
MQTT_PORT = int(os.environ.get("MQTT_PORT", 1883))
MQTT_USER = os.environ.get("MQTT_USER", "lap1")
MQTT_PASS = os.environ.get("MQTT_PASS", "HARI@MQTT")

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "telemetry.db")

def init_db():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            temperature REAL,
            humidity REAL,
            voltage REAL,
            rssi INTEGER,
            free_heap INTEGER,
            uptime INTEGER,
            raw_json TEXT
        )
    """)
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_device_time ON telemetry (device_id, id DESC)")
    conn.commit()
    conn.close()
    print(f"[SQLite Logger] Database initialized at {DB_PATH}")

def log_telemetry(payload_str):
    try:
        data = json.loads(payload_str)
        device_id = data.get("device_id", "unknown")
        temp = data.get("temperature")
        hum = data.get("humidity")
        volt = data.get("voltage")
        rssi = data.get("rssi")
        heap = data.get("free_heap")
        uptime = data.get("uptime")

        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        cursor.execute("""
            INSERT INTO telemetry (device_id, temperature, humidity, voltage, rssi, free_heap, uptime, raw_json)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (device_id, temp, hum, volt, rssi, heap, uptime, payload_str))
        conn.commit()
        conn.close()
        print(f"[SQLite Logger] Logged data for '{device_id}': T={temp}°C, H={hum}%, RSSI={rssi}dBm")
    except Exception as e:
        print(f"[SQLite Logger Error] Failed to insert telemetry row: {e}")

def handle_query(client, payload_str):
    try:
        req = json.loads(payload_str)
        device_id = req.get("device_id")
        limit = int(req.get("limit", 50))
        if limit > 500:
            limit = 500

        conn = sqlite3.connect(DB_PATH)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()

        if device_id:
            cursor.execute("""
                SELECT id, device_id, datetime(timestamp, 'localtime') as timestamp, temperature, humidity, voltage, rssi, free_heap, uptime 
                FROM telemetry WHERE device_id=? ORDER BY id DESC LIMIT ?
            """, (device_id, limit))
        else:
            cursor.execute("""
                SELECT id, device_id, datetime(timestamp, 'localtime') as timestamp, temperature, humidity, voltage, rssi, free_heap, uptime 
                FROM telemetry ORDER BY id DESC LIMIT ?
            """, (limit,))

        rows = [dict(r) for r in cursor.fetchall()]
        # Reverse so oldest comes first for sequential line charts
        rows.reverse()
        conn.close()

        response_payload = json.dumps({
            "status": "success",
            "count": len(rows),
            "device_id": device_id,
            "records": rows
        })
        client.publish("telemetry/history/response", response_payload)
        print(f"[SQLite Logger] Responded to query with {len(rows)} historical records.")
    except Exception as e:
        print(f"[SQLite Logger Error] Query processing failed: {e}")
        client.publish("telemetry/history/response", json.dumps({"status": "error", "message": str(e)}))

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT Connected] Bound to broker at {MQTT_HOST}:{MQTT_PORT}")
        client.subscribe("telemetry/+/data")
        client.subscribe("telemetry/query")
    else:
        print(f"[MQTT Connection Failed] Return code: {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore")
    
    if topic.endswith("/data"):
        log_telemetry(payload)
    elif topic == "telemetry/query":
        handle_query(client, payload)

def main():
    init_db()
    
    client = mqtt.Client(client_id="sqlite-telemetry-logger")
    if MQTT_USER and MQTT_PASS:
        client.username_pw_set(MQTT_USER, MQTT_PASS)

    client.on_connect = on_connect
    client.on_message = on_message

    def shutdown(sig, frame):
        print("\n[SQLite Logger] Shutting down cleanly...")
        client.disconnect()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    print(f"[SQLite Logger] Connecting to MQTT Broker at {MQTT_HOST}:{MQTT_PORT}...")
    try:
        client.connect(MQTT_HOST, MQTT_PORT, 60)
        client.loop_forever()
    except Exception as e:
        print(f"[SQLite Logger Fatal Error] {e}")

if __name__ == "__main__":
    main()
