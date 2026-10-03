# Termux Setup Guide: Remote Android Phone Control via MQTT

This guide walks you through setting up **Termux** on your Android phone to connect to the MQTT Hub. Once connected, your phone can receive commands from your laptop or the [Web UI Dashboard](file:///home/ha-r1/hub-ui/index.html).

---

## 1. Install Required Android Apps
- **Termux** (Download from F-Droid or GitHub Releases)
- **Termux:API** (Required for notifications, camera, battery, and TTS)

---

## 2. Install Packages in Termux
Open Termux on your Android phone and run the following command:

```bash
pkg update && pkg upgrade -y
pkg install python git termux-api android-tools -y
pip install paho-mqtt psutil
```

---

## 3. Grant Required Permissions
Run storage setup in Termux:
```bash
termux-setup-storage
```
Then go to **Android Settings → Apps → Termux:API → Permissions** and enable:
- 📷 Camera
- 🔔 Notifications
- 📁 Storage

---

## 4. Copy `agent.py` to Your Phone
Copy `agent.py` from your laptop to your phone's Termux home directory:

```bash
# Option A: Run a temporary Python HTTP server on laptop
# On laptop (in directory with agent.py):
python3 -m http.server 8000

# On Termux:
curl -O http://<LAPTOP_IP>:8000/agent.py
```

---

## 5. Enable Wireless ADB (Optional: for Taps, Swipes & Screen Capture)
To enable touch tapping, swipes, phone keys (Home/Back/Recents), and screen capture:

1. On Phone: Go to **Settings → Developer Options → Wireless Debugging** (Turn ON).
2. Tap **Pair device with pairing code**.
3. In Termux (or on laptop):
   ```bash
   adb pair <PHONE_IP>:<PAIR_PORT> <PAIRING_CODE>
   adb connect <PHONE_IP>:<CONNECT_PORT>
   adb devices
   ```

---

## 6. Run the Agent on Your Phone

In Termux, set your environment variables and start `agent.py`:

```bash
export DEVICE_NAME="phone"
export MQTT_HOST="100.80.220.22"   # Replace with your broker IP
export MQTT_USER="phone"
export MQTT_PASS="your_password"   # Replace with your MQTT password

python3 ~/agent.py
```

---

## 7. Phone Control Features Available in Web UI
Once `agent.py` is running on your phone, it appears in the Web Dashboard with real-time controls:

| Category | Available Commands |
| :--- | :--- |
| **Notifications & Speech** | Send push notifications (`notify`) and speak text out loud (`tts`) |
| **Screen & Camera** | Remote camera photos (`camera`) and screen capture (`screenshot`) |
| **Phone Keys** | `🏠 Home`, `◀ Back`, `▢ Recents`, `Vol +`, `Vol -` |
| **Touch Controls** | Tap exact X Y screen coordinates (`tap <x> <y>`) and swipes |
| **System Info** | Real-time battery level and phone status (`status` / `stats`) |
