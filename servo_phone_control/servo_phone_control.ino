#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define SERVO_PIN 18
#define MIN_US 600            // calibrate: narrow if the servo strains at the ends
#define MAX_US 2400
#define RANGE_DEG 270
#define STEP_DEG 2            // degrees per 15 ms tick (~130 deg/s)
#define CAM_TIMEOUT_MS 1500   // no heartbeat for this long -> treat as obstacle

WebServer server(80);

volatile int targetAngle = 135;
volatile int currentAngle = 135;
volatile bool camObstacle = false;   // fist seen by the camera
volatile bool camLost = false;       // heartbeat timed out
volatile bool camSeen = false;       // set after the first heartbeat
volatile uint32_t lastBeat = 0;

inline bool blocked() { return camObstacle || camLost; }

void servoWriteAngle(int deg) {
  deg = constrain(deg, 0, RANGE_DEG);
  uint32_t us = map(deg, 0, RANGE_DEG, MIN_US, MAX_US);
  ledcWrite(SERVO_PIN, us * 65535UL / 20000UL);
}

// Moves the servo toward targetAngle in small steps; freezes instantly on obstacle.
void servoTask(void*) {
  bool wasBlocked = false;
  for (;;) {
    bool b = blocked();
    if (b && !wasBlocked) targetAngle = currentAngle;   // freeze where it is
    wasBlocked = b;
    if (!b) {
      int t = targetAngle, c = currentAngle;
      if (c != t) {
        c += constrain(t - c, -STEP_DEG, STEP_DEG);
        currentAngle = c;
        servoWriteAngle(c);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}

// Marks the camera as lost if heartbeats stop.
void watchdogTask(void*) {
  for (;;) {
    camLost = camSeen && (millis() - lastBeat > CAM_TIMEOUT_MS);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

const char PAGE[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:sans-serif;text-align:center;padding:24px}
#warn{display:none;background:#c62828;color:#fff;padding:16px;border-radius:8px;font-size:20px;margin-bottom:20px}
input{width:100%;height:48px}
</style></head><body>
<div id="warn"></div>
<h2>Servo: <span id="v">135</span>&deg;</h2>
<input id="s" type="range" min="0" max="270" value="135">
<p style="color:#777;font-size:13px">Tap the page once to enable alert sound.</p>
<script>
const s=document.getElementById('s'),v=document.getElementById('v'),w=document.getElementById('warn');
let busy=false,prev=false,ac=null;
function enableAudio(){if(!ac)ac=new (window.AudioContext||window.webkitAudioContext)();}
document.addEventListener('touchstart',enableAudio);document.addEventListener('click',enableAudio);
function beep(){if(!ac)return;const o=ac.createOscillator();o.frequency.value=880;o.connect(ac.destination);o.start();setTimeout(()=>o.stop(),250);}
s.oninput=()=>{v.textContent=s.value;if(busy)return;busy=true;
 fetch('/set?a='+s.value).finally(()=>setTimeout(()=>busy=false,50));};
setInterval(()=>fetch('/status').then(r=>r.json()).then(d=>{
 if(d.obstacle){
  w.textContent='OBSTACLE AHEAD ('+d.reason+') - SERVO STOPPED';w.style.display='block';
  s.disabled=true;s.value=d.angle;v.textContent=d.angle;
  if(!prev){if(navigator.vibrate)navigator.vibrate([200,100,200]);beep();}
 }else{
  w.style.display='none';s.disabled=false;
  if(prev){s.value=d.angle;v.textContent=d.angle;}
 }
 prev=d.obstacle;
}).catch(()=>{}),300);
</script></body></html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  ledcAttach(SERVO_PIN, 50, 16);
  servoWriteAngle(currentAngle);

  WiFi.softAP("ServoCtrl", "12345678");
  Serial.print("Open http://"); Serial.println(WiFi.softAPIP());   // normally 192.168.4.1

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });

  server.on("/set", []() {
    if (blocked()) { server.send(409, "text/plain", "blocked"); return; }
    targetAngle = constrain(server.arg("a").toInt(), 0, RANGE_DEG);
    server.send(200, "text/plain", "ok");
  });

  server.on("/obstacle", []() {            // /obstacle?s=1 or s=0, also the heartbeat
    camObstacle = server.arg("s") == "1";
    lastBeat = millis();
    camSeen = true;
    server.send(200, "text/plain", "ok");
  });

  server.on("/status", []() {
    bool b = blocked();
    const char* reason = camLost ? "camera lost" : (camObstacle ? "fist" : "");
    server.send(200, "application/json",
      String("{\"obstacle\":") + (b ? "true" : "false") +
      ",\"reason\":\"" + reason + "\"" +
      ",\"angle\":" + (int)currentAngle + "}");
  });

  server.begin();

  xTaskCreate(servoTask,    "servo", 3072, NULL, 3, NULL);
  xTaskCreate(watchdogTask, "wdog",  2048, NULL, 2, NULL);
}

void loop() {                      // web server runs here at normal priority
  server.handleClient();
  vTaskDelay(pdMS_TO_TICKS(2));
}
