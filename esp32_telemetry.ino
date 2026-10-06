/*
  ESP32 MQTT Telemetry & Remote Control Firmware
  Project: Hands-Free Device Control Over MQTT
  Author: Antigravity Agent
  
  Dependencies (Install via Arduino Library Manager):
  - PubSubClient (by Nick O'Leary)
  - ArduinoJson (by Benoit Blanchon, v6 or v7)
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= CONFIGURATION =======================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_SERVER   = "192.168.1.82"; // Laptop Mosquitto Broker IP
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "lap1";
const char* MQTT_PASS     = "HARI@MQTT";

const char* DEVICE_ID     = "esp32_node1";
const char* TELEMETRY_TOPIC = "telemetry/esp32_node1/data";
const char* COMMAND_TOPIC   = "devices/esp32_node1/cmd";
const char* STATUS_TOPIC    = "devices/esp32_node1/status";
const char* REPLY_TOPIC     = "devices/esp32_node1/reply";

const int LED_PIN = 2; // Built-in LED on most ESP32 boards
const unsigned long TELEMETRY_INTERVAL_MS = 3000; // Publish every 3s
// =============================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastTelemetryTime = 0;

// Simulated or analog sensor reading functions
float readTemperature() {
  // Replace with actual sensor reading e.g. dht.readTemperature()
  static float temp = 25.0;
  temp += ((random(-50, 50)) / 100.0);
  if (temp < 15.0) temp = 15.0;
  if (temp > 40.0) temp = 40.0;
  return temp;
}

float readHumidity() {
  // Replace with actual sensor reading e.g. dht.readHumidity()
  static float hum = 50.0;
  hum += ((random(-100, 100)) / 100.0);
  if (hum < 20.0) hum = 20.0;
  if (hum > 90.0) hum = 90.0;
  return hum;
}

float readVoltage() {
  // Replace with battery / analog pin reading e.g. (analogRead(34) / 4095.0) * 3.3 * 2.0
  return 3.20 + (random(0, 20) / 100.0);
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
  Serial.print("IP Address: ");
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
    String reply = "ESP32 Node | IP: " + WiFi.localIP().toString() + 
                   " | Heap: " + String(ESP.getFreeHeap()) + " B" +
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
      mqttClient.publish(REPLY_TOPIC, "ESP32 telemetry node online");
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
  Serial.print("Published telemetry: ");
  Serial.println(jsonBuffer);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);

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
