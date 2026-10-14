import cv2, time, math, requests
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision

ESP = "http://192.168.4.1"
ON_N, OFF_N = 3, 6

options = vision.HandLandmarkerOptions(
    base_options=mp_python.BaseOptions(model_asset_path="hand_landmarker.task"),
    running_mode=vision.RunningMode.VIDEO,
    num_hands=1,
    min_hand_detection_confidence=0.6,
    min_tracking_confidence=0.5,
)
landmarker = vision.HandLandmarker.create_from_options(options)

cap = cv2.VideoCapture(0)
sess = requests.Session()
state, on_cnt, off_cnt, last_send = False, 0, 0, 0
t0 = time.time()

def send(s):
    try: sess.get(f"{ESP}/obstacle", params={"s": int(s)}, timeout=0.5)
    except requests.RequestException: pass

def d(a, b): return math.hypot(a.x - b.x, a.y - b.y)

def is_fist(lm):
    wrist = lm[0]
    curled = sum(d(lm[t], wrist) < d(lm[p], wrist)
                 for t, p in [(8, 6), (12, 10), (16, 14), (20, 18)])
    return curled == 4

while True:
    ok, frame = cap.read()
    if not ok: break
    h, w = frame.shape[:2]
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    res = landmarker.detect_for_video(img, int((time.time() - t0) * 1000))

    hit = False
    for lm in res.hand_landmarks:
        hit |= is_fist(lm)
        for p in lm:
            cv2.circle(frame, (int(p.x * w), int(p.y * h)), 4, (255, 200, 0), -1)

    if hit: on_cnt += 1; off_cnt = 0
    else:   off_cnt += 1; on_cnt = 0
    if not state and on_cnt >= ON_N: state = True
    if state and off_cnt >= OFF_N: state = False

    if time.time() - last_send > 0.3:      # heartbeat + state
        send(state); last_send = time.time()

    cv2.putText(frame, "FIST - OBSTACLE" if state else "clear", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9,
                (0, 0, 255) if state else (0, 200, 0), 2)
    cv2.imshow("fist obstacle", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

cap.release()
cv2.destroyAllWindows()
