#include <Arduino.h>

#define SERVO_PIN 18
#define MIN_US 600      // start conservative, widen after testing
#define MAX_US 2400
#define RANGE_DEG 270

void servoWriteUs(int us) {
  // 50 Hz, 16-bit: period = 20000 us
  ledcWrite(SERVO_PIN, (uint32_t)us * 65535UL / 20000UL);
}

void servoWriteAngle(int deg) {
  deg = constrain(deg, 0, RANGE_DEG);
  servoWriteUs(map(deg, 0, RANGE_DEG, MIN_US, MAX_US));
}

void setup() {
  Serial.begin(115200);
  ledcAttach(SERVO_PIN, 50, 16);
  servoWriteAngle(135);          // center
  delay(1000);
}

void loop() {
  for (int a = 135; a >= 0; a -= 1)   { servoWriteAngle(a); delay(15); }
  for (int a = 0; a <= 270; a += 1)   { servoWriteAngle(a); delay(15); }
  for (int a = 270; a >= 135; a -= 1) { servoWriteAngle(a); delay(15); }
  delay(1000);
}