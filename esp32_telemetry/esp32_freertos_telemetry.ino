/*
  ESP32 FreeRTOS Dual-Core Telemetry Firmware
  Project: Hands-Free Device Control Over MQTT with FreeRTOS
  
  Architecture:
  - Task 1 (Core 0, Priority 1): Reads DHT11 Sensor on GPIO 15 every 2 seconds & pushes telemetry structs into a FreeRTOS Queue.
  - Task 2 (Core 1, Priority 2): Handles MQTT network loop, listens for commands, and publishes telemetry from the Queue.
  - Inter-Task Communication: FreeRTOS QueueHandle_t (Thread-Safe, Non-Blocking).
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
const char* WIFI_SSID     = "SMEC_R&D-4G";        // Update to your Wi-Fi SSID
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // Update to your Wi-Fi Password

const char* MQTT_SERVER   = "192.168.1.119"; // Laptop Mosquitto Broker IP
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "lap1";
const char* MQTT_PASS     = "HARI@MQTT";

const char* DEVICE_ID       = "esp32_node1";
const char* TELEMETRY_TOPIC = "telemetry/esp32_node1/data";
const char* COMMAND_TOPIC   = "devices/esp32_node1/cmd";
const char* STATUS_TOPIC    = "devices/esp32_node1/status";
const char* REPLY_TOPIC     = "devices/esp32_node1/reply";
// =====================================================================

// FreeRTOS Data Structure for Telemetry
typedef struct {
  float temperature;
  float humidity;
  float voltage;
  int rssi;
  uint32_t freeHeap;
  unsigned long uptime;
} TelemetryData_t;

// FreeRTOS Handles
QueueHandle_t telemetryQueue;
TaskHandle_t TaskSensorHandle;
TaskHandle_t TaskMQTTHandle;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

float lastValidTemp = 25.0;
float lastValidHum = 50.0;

// MQTT Command Callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.printf("[FreeRTOS Core %d] Command received [%s]: %s\n", xPortGetCoreID(), topic, message.c_str());

  if (message.equalsIgnoreCase("led on") || message.equalsIgnoreCase("lock")) {
    digitalWrite(LED_PIN, HIGH);
    mqttClient.publish(REPLY_TOPIC, "ESP32 FreeRTOS LED Turned ON");
  } 
  else if (message.equalsIgnoreCase("led off")) {
    digitalWrite(LED_PIN, LOW);
    mqttClient.publish(REPLY_TOPIC, "ESP32 FreeRTOS LED Turned OFF");
  } 
  else if (message.equalsIgnoreCase("ping")) {
    mqttClient.publish(REPLY_TOPIC, "pong from FreeRTOS ESP32");
  } 
  else if (message.equalsIgnoreCase("status") || message.equalsIgnoreCase("stats")) {
    String reply = "ESP32 FreeRTOS Dual-Core | IP: " + WiFi.localIP().toString() + 
                   " | Temp: " + String(lastValidTemp) + "C" +
                   " | Hum: " + String(lastValidHum) + "%" +
                   " | Heap: " + String(ESP.getFreeHeap()) + " B";
    mqttClient.publish(REPLY_TOPIC, reply.c_str());
  } 
  else {
    mqttClient.publish(REPLY_TOPIC, ("Unknown command: " + message).c_str());
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("[FreeRTOS Core %d] Connecting to Mosquitto Broker...\n", xPortGetCoreID());
    if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASS, STATUS_TOPIC, 0, true, "offline")) {
      Serial.println("[FreeRTOS] MQTT Connected!");
      mqttClient.publish(STATUS_TOPIC, "online", true);
      mqttClient.subscribe(COMMAND_TOPIC);
      mqttClient.publish(REPLY_TOPIC, "ESP32 FreeRTOS node online");
    } else {
      Serial.printf("[FreeRTOS] Connection failed, rc=%d. Retrying in 3s...\n", mqttClient.state());
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}

// ======================= FREERTOS TASK 1 (CORE 0): SENSOR SAMPLING =======================
void TaskSensorCode(void* pvParameters) {
  Serial.printf("[FreeRTOS] TaskSensor started on Core %d\n", xPortGetCoreID());
  dht.begin();

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(2000); // Sample every 2000ms

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) lastValidTemp = t;
    else t = lastValidTemp;

    if (!isnan(h)) lastValidHum = h;
    else h = lastValidHum;

    TelemetryData_t data;
    data.temperature = t;
    data.humidity = h;
    data.voltage = 3.30;
    data.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    data.freeHeap = ESP.getFreeHeap();
    data.uptime = millis() / 1000;

    // Send data to FreeRTOS Queue (non-blocking if queue full)
    if (xQueueSend(telemetryQueue, &data, (TickType_t)0) != pdPASS) {
      Serial.println("[FreeRTOS Core 0 Warning] Telemetry Queue Full!");
    } else {
      Serial.printf("[FreeRTOS Core 0] Sampled DHT11: T=%.1fC, H=%.1f%%\n", t, h);
    }
  }
}

// ======================= FREERTOS TASK 2 (CORE 1): MQTT NETWORKING =======================
void TaskMQTTCode(void* pvParameters) {
  Serial.printf("[FreeRTOS] TaskMQTT started on Core %d\n", xPortGetCoreID());

  // Setup Wi-Fi connection on Core 1
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.printf("\n[FreeRTOS] Wi-Fi Connected! IP: %s\n", WiFi.localIP().toString().c_str());

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  TelemetryData_t receivedData;

  for (;;) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();

    // Check if new telemetry data is waiting in FreeRTOS Queue
    if (xQueueReceive(telemetryQueue, &receivedData, pdMS_TO_TICKS(50)) == pdTRUE) {
      StaticJsonDocument<256> doc;
      doc["device_id"] = DEVICE_ID;
      doc["temperature"] = round(receivedData.temperature * 10.0) / 10.0;
      doc["humidity"] = round(receivedData.humidity * 10.0) / 10.0;
      doc["voltage"] = round(receivedData.voltage * 100.0) / 100.0;
      doc["rssi"] = receivedData.rssi;
      doc["free_heap"] = receivedData.freeHeap;
      doc["uptime"] = receivedData.uptime;

      char jsonBuffer[256];
      serializeJson(doc, jsonBuffer);

      mqttClient.publish(TELEMETRY_TOPIC, jsonBuffer);
      Serial.printf("[FreeRTOS Core 1] Published to MQTT: %s\n", jsonBuffer);
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Give CPU yield time
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);
  delay(1000);

  Serial.println("====================================================");
  Serial.println("  ESP32 FreeRTOS Dual-Core Telemetry System");
  Serial.println("====================================================");

  // Create FreeRTOS Queue (capacity: 10 TelemetryData_t items)
  telemetryQueue = xQueueCreate(10, sizeof(TelemetryData_t));

  if (telemetryQueue == NULL) {
    Serial.println("[FreeRTOS Fatal Error] Queue Creation Failed!");
    return;
  }

  // Create Task 1 on Core 0 (Sensor Sampling)
  xTaskCreatePinnedToCore(
    TaskSensorCode,   // Task Function
    "TaskSensor",     // Task Name
    4096,             // Stack Size (bytes)
    NULL,             // Parameters
    1,                // Priority (1 = Low/Medium)
    &TaskSensorHandle,// Task Handle
    0                 // Core 0
  );

  // Create Task 2 on Core 1 (MQTT & Wi-Fi Networking)
  xTaskCreatePinnedToCore(
    TaskMQTTCode,     // Task Function
    "TaskMQTT",       // Task Name
    8192,             // Stack Size (bytes)
    NULL,             // Parameters
    2,                // Priority (2 = Higher Priority for Network)
    &TaskMQTTHandle,  // Task Handle
    1                 // Core 1
  );

  Serial.println("[FreeRTOS] Tasks pinned to Core 0 & Core 1 successfully!");
}

void loop() {
  // Empty! FreeRTOS scheduler manages all tasks concurrently.
  vTaskDelete(NULL); 
}
