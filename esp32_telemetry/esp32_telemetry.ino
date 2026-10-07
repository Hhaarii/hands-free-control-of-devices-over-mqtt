/*
  ESP32 MQTT Telemetry & Remote Control Firmware
  Project: Hands-Free Device Control Over MQTT
  Hardware Setup:
  - DHT11 Sensor: VCC -> 3.3V, GND -> GND, Data Out -> GPIO 15 (D15)
  
  Dependencies (Installed in ~/Arduino/libraries):
  - PubSubClient (by Nick O'Leary)
  - ArduinoJson (by Benoit Blanchon)
  - DHT sensor library (by Adafruit)
  - Adafruit Unified Sensor (by Adafruit)
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// ======================= HARDWARE PIN CONFIG =======================
#define DHTPIN 15       // Digital Out pin connected to GPIO 15 (D15)
#define DHTTYPE DHT11   // DHT 11
const int LED_PIN = 2;  // Built-in LED on ESP32 board
// ===================================================================

// ======================= NETWORK CONFIGURATION =======================
const char* WIFI_SSID     = "SMEC_R&D-4G"; // Update to your Wi-Fi SSID
const char* WIFI_PASSWORD = "smec_r&d@7714"; // Update to your Wi-Fi Password

const char* MQTT_SERVER   = "192.168.1.119"; // Laptop Mosquitto Broker IP (or 192.168.1.82)
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "lap1";
const char* MQTT_PASS     = "HARI@MQTT";

const char* DEVICE_ID       = "esp32_node1";
const char* TELEMETRY_TOPIC = "telemetry/esp32_node1/data";
const char* COMMAND_TOPIC   = "devices/esp32_node1/cmd";
const char* STATUS_TOPIC    = "devices/esp32_node1/status";
const char* REPLY_TOPIC     = "devices/esp32_node1/reply";

const unsigned long TELEMETRY_INTERVAL_MS = 2000; // Publish every 2 seconds
// =====================================================================

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastTelemetryTime = 0;
float lastValidTemp = 25.0;
float lastValidHum = 50.0;

// Read real-time Temperature from DHT11 on GPIO 15
float readTemperature() {
  float t = dht.readTemperature(); // Read temp in Celsius
  if (isnan(t)) {
    Serial.println("[DHT11 Warning] Failed to read temperature! Using last valid value.");
    return lastValidTemp;
  }
  lastValidTemp = t;
  return t;
}

// Read real-time Humidity from DHT11 on GPIO 15
float readHumidity() {
  float h = dht.readHumidity(); // Read humidity in %
  if (isnan(h)) {
    Serial.println("[DHT11 Warning] Failed to read humidity! Using last valid value.");
    return lastValidHum;
  }
  lastValidHum = h;
  return h;
}

float readVoltage() {
  // Approximate supply voltage or ADC battery reading
  return 3.30;
}

void setupWiFi() {
  delay(10);
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.print("Command received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (message.equalsIgnoreCase("led on") || message.equalsIgnoreCase("lock")) {
    digitalWrite(LED_PIN, HIGH);
    mqttClient.publish(REPLY_TOPIC, "ESP32 LED Turned ON");
  } 
  else if (message.equalsIgnoreCase("led off")) {
    digitalWrite(LED_PIN, LOW);
    mqttClient.publish(REPLY_TOPIC, "ESP32 LED Turned OFF");
  } 
  else if (message.equalsIgnoreCase("ping")) {
    mqttClient.publish(REPLY_TOPIC, "pong from ESP32");
  } 
  else if (message.equalsIgnoreCase("status") || message.equalsIgnoreCase("stats")) {
    String reply = "ESP32 DHT11 Node | IP: " + WiFi.localIP().toString() + 
                   " | Temp: " + String(lastValidTemp) + "C" +
                   " | Hum: " + String(lastValidHum) + "%" +
                   " | RSSI: " + String(WiFi.RSSI()) + " dBm";
    mqttClient.publish(REPLY_TOPIC, reply.c_str());
  } 
  else {
    mqttClient.publish(REPLY_TOPIC, ("Unknown command: " + message).c_str());
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Connect with Last Will & Testament (LWT)
    if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASS, STATUS_TOPIC, 0, true, "offline")) {
      Serial.println("Connected to Mosquitto Broker!");
      mqttClient.publish(STATUS_TOPIC, "online", true);
      mqttClient.subscribe(COMMAND_TOPIC);
      mqttClient.publish(REPLY_TOPIC, "ESP32 DHT11 telemetry node online");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void sendTelemetry() {
  float temp = readTemperature();
  float hum = readHumidity();
  float volt = readVoltage();
  int rssi = WiFi.RSSI();
  uint32_t freeHeap = ESP.getFreeHeap();
  unsigned long uptime = millis() / 1000;

  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["temperature"] = round(temp * 10.0) / 10.0;
  doc["humidity"] = round(hum * 10.0) / 10.0;
  doc["voltage"] = round(volt * 100.0) / 100.0;
  doc["rssi"] = rssi;
  doc["free_heap"] = freeHeap;
  doc["uptime"] = uptime;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  mqttClient.publish(TELEMETRY_TOPIC, jsonBuffer);
  Serial.print("Published DHT11 Telemetry: ");
  Serial.println(jsonBuffer);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);

  // Initialize DHT11 sensor on GPIO 15
  dht.begin();
  Serial.println("DHT11 Sensor Initialized on GPIO 15 (Pin D15)");

  setupWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;
    sendTelemetry();
  }
}
