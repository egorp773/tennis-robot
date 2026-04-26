#!/usr/bin/env python3
"""
Tennis Ball Detection + TSP Path Planner
Raspberry Pi 4 + Camera Module 3 NoIR Wide + YOLOv8n

Детектор гибридный:
  1. HSV цветовой фильтр (жёлто-зелёный цвет мяча) — быстро, надёжно
  2. YOLOv8n sports-ball класс — как дополнение
  Результаты объединяются, дубликаты удаляются по IoU.

Использование:
  python3 main.py --no-display   # headless + MJPEG стрим http://PI_IP:8080
  python3 main.py --test         # один кадр и выйти
  python3 main.py                # с local display
"""

import argparse
import math
import os
import socket
import sys
import time
import threading
import serial
from http.server import BaseHTTPRequestHandler, HTTPServer

import cv2
import numpy as np
from picamera2 import Picamera2
from ultralytics import YOLO

# ─────────────────────────── КОНСТАНТЫ ──────────────────────────────

SPORTS_BALL_CLASS = 32          # COCO class id для sports ball
FRAME_W, FRAME_H  = 640, 480
FRAME_CX, FRAME_CY = FRAME_W // 2, FRAME_H // 2   # 320, 240

COLLECT_SIZE_PX2  = 4000        # площадь bbox — команда COLLECT
YOLO_CONF         = 0.30        # порог уверенности YOLO
YOLO_IMGSZ        = 416         # 416px → лучше видит мелкие мячи (~3-5 FPS)

# HSV диапазон теннисного мяча (жёлто-зелёный "optic yellow")
# OpenCV H: 0-180, S: 0-255, V: 0-255
# Тесный диапазон — только характерный жёлто-зелёный цвет мяча
HSV_LOWER = np.array([26, 110, 100])
HSV_UPPER = np.array([56, 255, 255])
HSV_MIN_AREA    = 300   # пикселей² — минимальный мяч (~20×20 px)
HSV_MAX_AREA    = 60000 # пикселей² — максимальный мяч (вблизи)
HSV_CIRCULARITY = 0.55  # строже: мяч должен быть хорошо округлым
HSV_MAX_ASPECT  = 1.7   # ширина/высота — не слишком вытянутый объект
HSV_MIN_FILL    = 0.60  # минимум — контур заполняет 60% описанного круга

IOU_MERGE_THRESH = 0.3  # объединяем bbox если IoU > этого порога

COLOR_HSV  = (0, 255, 0)        # зелёный — HSV детекция
COLOR_YOLO = (0, 200, 255)      # жёлтый — YOLO детекция
COLOR_TEXT = (255, 255, 255)
COLOR_ROUTE = (0, 140, 255)

# ─────────────────────────── ГЛОБАЛЫ ────────────────────────────────

_frame_lock  = threading.Lock()
_latest_jpeg = None

uart       = None
balls_full = False
robot_mode = 'SEARCH'
_mode_lock = threading.Lock()


# ─────────────────────────── UART ───────────────────────────────────

def init_uart():
    global uart
    for port in ('/dev/ttyAMA0', '/dev/serial0'):
        try:
            uart = serial.Serial(port, baudrate=115200, timeout=1)
            print(f"UART открыт: {port}", flush=True)
            threading.Thread(target=_uart_reader, daemon=True).start()
            return
        except Exception as e:
            print(f"UART {port} недоступен: {e}", flush=True)
    uart = None
    print("UART недоступен — работаем без Serial.", flush=True)


def _uart_reader():
    global balls_full, uart, robot_mode
    while uart is not None:
        try:
            line = uart.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue
            print(f"Serial IN: {line}", flush=True)
            if line == "BALLS_FULL":
                balls_full = True
            elif line.startswith("MODE:"):
                with _mode_lock:
                    robot_mode = line[5:]
                print(f"Serial: ESP32 режим → {robot_mode}", flush=True)
        except Exception as e:
            print(f"Serial reader error: {e}", flush=True)
            break


def send_uart(msg: str):
    global uart
    if uart is not None:
        try:
            uart.write((msg + '\n').encode('utf-8'))
        except Exception as e:
            print(f"Serial write error: {e}", flush=True)


# ─────────────────────────── MJPEG ──────────────────────────────────

class MJPEGHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def do_GET(self):
        if self.path in ('/', '/stream'):
            self.send_response(200)
            self.send_header('Content-Type',
                             'multipart/x-mixed-replace; boundary=frame')
            self.end_headers()
            try:
                while True:
                    with _frame_lock:
                        jpeg = _latest_jpeg
                    if jpeg:
                        self.wfile.write(b'--frame\r\n')
                        self.wfile.write(b'Content-Type: image/jpeg\r\n\r\n')
                        self.wfile.write(jpeg)
                        self.wfile.write(b'\r\n')
                    time.sleep(0.04)
            except (BrokenPipeError, ConnectionResetError):
                pass
        else:
            html = (
                b"<!DOCTYPE html><html><head><title>Tennis Bot</title>"
                b"<style>body{background:#111;color:#eee;font-family:monospace;"
                b"text-align:center}img{max-width:100%;border:2px solid #0f0}</style></head>"
                b"<body><h2>Tennis Ball Detection</h2><img src='/stream'><br>"
                b"<small>HSV + YOLOv8n | Raspberry Pi 4</small></body></html>"
            )
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(html)


def start_mjpeg_server(port=8080):
    server = HTTPServer(('0.0.0.0', port), MJPEGHandler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    print(f"MJPEG стрим: http://{socket.gethostname()}.local:{port}/", flush=True)
    return server


def push_frame(frame_bgr):
    global _latest_jpeg
    ok, buf = cv2.imencode('.jpg', frame_bgr, [cv2.IMWRITE_JPEG_QUALITY, 75])
    if ok:
        with _frame_lock:
            _latest_jpeg = buf.tobytes()


# ─────────────────────────── TSP ────────────────────────────────────

def nearest_neighbor_tsp(points):
    if not points:
        return [], 0.0
    if len(points) == 1:
        return list(points), 0.0
    unvisited = list(range(len(points)))
    route = [unvisited.pop(0)]
    while unvisited:
        cx, cy = points[route[-1]]
        best_d, best_i = float('inf'), None
        for i in unvisited:
            d = math.hypot(points[i][0] - cx, points[i][1] - cy)
            if d < best_d:
                best_d, best_i = d, i
        route.append(best_i)
        unvisited.remove(best_i)
    total = sum(
        math.hypot(points[route[i+1]][0] - points[route[i]][0],
                   points[route[i+1]][1] - points[route[i]][1])
        for i in range(len(route) - 1)
    )
    return [points[i] for i in route], total


# ─────────────────────────── ДЕТЕКТОРЫ ──────────────────────────────

def _iou(a, b):
    """Intersection over Union двух bbox."""
    ix1 = max(a['x1'], b['x1'])
    iy1 = max(a['y1'], b['y1'])
    ix2 = min(a['x2'], b['x2'])
    iy2 = min(a['y2'], b['y2'])
    if ix2 <= ix1 or iy2 <= iy1:
        return 0.0
    inter = (ix2 - ix1) * (iy2 - iy1)
    area_a = (a['x2']-a['x1']) * (a['y2']-a['y1'])
    area_b = (b['x2']-b['x1']) * (b['y2']-b['y1'])
    return inter / (area_a + area_b - inter + 1e-6)


def merge_detections(lists):
    """Объединяет списки детекций, удаляет перекрывающиеся (IoU > порога)."""
    all_balls = [b for lst in lists for b in lst]
    kept = []
    for ball in sorted(all_balls, key=lambda b: -b['conf']):
        if all(_iou(ball, k) < IOU_MERGE_THRESH for k in kept):
            kept.append(ball)
    return kept


def detect_by_color(frame_bgr):
    """HSV цветовой детектор теннисных мячей. Быстро, не требует модели."""
    hsv  = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2HSV)
    mask = cv2.inRange(hsv, HSV_LOWER, HSV_UPPER)

    # Морфология — убираем шум
    k5 = np.ones((5, 5), np.uint8)
    k3 = np.ones((3, 3), np.uint8)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, k5)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN,  k3)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL,
                                   cv2.CHAIN_APPROX_SIMPLE)
    balls = []
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if not (HSV_MIN_AREA <= area <= HSV_MAX_AREA):
            continue
        peri = cv2.arcLength(cnt, True)
        if peri < 1:
            continue
        circ = 4 * math.pi * area / (peri * peri)
        if circ < HSV_CIRCULARITY:
            continue
        x, y, w, h = cv2.boundingRect(cnt)
        # Фильтр по соотношению сторон — не вытянутые объекты
        aspect = max(w, h) / max(min(w, h), 1)
        if aspect > HSV_MAX_ASPECT:
            continue
        # Фильтр по заполнению описанного круга
        (_, _), radius = cv2.minEnclosingCircle(cnt)
        fill = area / max(math.pi * radius * radius, 1e-6)
        if fill < HSV_MIN_FILL:
            continue
        balls.append({
            'x': x + w // 2, 'y': y + h // 2,
            'x1': x, 'y1': y, 'x2': x + w, 'y2': y + h,
            'conf': round(circ, 2),
            'source': 'HSV',
        })
    return balls


class YoloDetector:
    def __init__(self, weights='yolov8n.pt', conf=YOLO_CONF):
        print(f"Загружаем YOLO модель: {weights} ...", flush=True)
        self.model = YOLO(weights)
        self.conf  = conf
        name = self.model.names.get(SPORTS_BALL_CLASS, '?')
        print(f"YOLO готов. class {SPORTS_BALL_CLASS} = '{name}'", flush=True)

    def detect(self, frame_rgb):
        results = self.model(frame_rgb, verbose=False,
                             conf=self.conf, imgsz=YOLO_IMGSZ)
        balls = []
        if results and results[0].boxes is not None:
            for box in results[0].boxes:
                if int(box.cls[0]) != SPORTS_BALL_CLASS:
                    continue
                x1, y1, x2, y2 = [int(v) for v in box.xyxy[0].tolist()]
                balls.append({
                    'x': (x1+x2)//2, 'y': (y1+y2)//2,
                    'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2,
                    'conf': float(box.conf[0]),
                    'source': 'YOLO',
                })
        return balls


# ─────────────────────────── ОТРИСОВКА ──────────────────────────────

def draw_detections(frame_bgr, balls, route):
    # Крестик в центре кадра
    cv2.drawMarker(frame_bgr, (FRAME_CX, FRAME_CY),
                   (0, 0, 255), cv2.MARKER_CROSS, 20, 2)

    for i, b in enumerate(balls):
        color = COLOR_HSV if b['source'] == 'HSV' else COLOR_YOLO
        cv2.rectangle(frame_bgr, (b['x1'], b['y1']),
                      (b['x2'], b['y2']), color, 2)
        cv2.drawMarker(frame_bgr, (b['x'], b['y']),
                       color, cv2.MARKER_CROSS, 14, 2)
        label = f"#{i+1} {b['source']} {b['conf']:.2f}"
        cv2.putText(frame_bgr, label, (b['x1'], b['y1'] - 6),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.45, COLOR_TEXT, 1, cv2.LINE_AA)

    if len(route) > 1:
        for i in range(len(route) - 1):
            cv2.arrowedLine(frame_bgr, route[i], route[i+1],
                            COLOR_ROUTE, 2, tipLength=0.15)

    cv2.putText(frame_bgr, f"Balls: {len(balls)}", (8, 24),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
    return frame_bgr


# ─────────────────────────── MAIN LOOP ──────────────────────────────

def run(no_display=False, test_mode=False, stream_port=8080):
    global balls_full

    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    init_uart()

    yolo = YoloDetector(weights='yolov8n.pt')
    cam  = CameraStream()

    if not no_display and not os.environ.get("DISPLAY"):
        no_display = True

    start_mjpeg_server(stream_port)
    print("\nДетекция запущена. Ctrl+C для остановки.\n", flush=True)

    frame_count = 0
    t0 = time.time()

    try:
        while True:
            frame_rgb = cam.read()
            frame_bgr = frame_rgb[:, :, ::-1].copy()

            # Детекция — только нейросеть (корт зелёный, HSV ненадёжен)
            balls = yolo.detect(frame_rgb)

            # ── UART ─────────────────────────────────────────────────
            with _mode_lock:
                mode = robot_mode

            if balls_full:
                send_uart("RETURN")
                print("Serial OUT: RETURN", flush=True)
                balls_full = False
            elif mode in ('COLLECTING', 'RETURNING'):
                pass  # ESP32 занят — молчим
            elif not balls:
                send_uart("SEARCH")
            else:
                nearest = min(balls, key=lambda b: math.hypot(
                    b['x'] - FRAME_CX, b['y'] - FRAME_CY))
                dx   = nearest['x'] - FRAME_CX
                size = (nearest['x2'] - nearest['x1']) * \
                       (nearest['y2'] - nearest['y1'])
                if size > COLLECT_SIZE_PX2:
                    send_uart("COLLECT")
                else:
                    send_uart(f"TRACK:{dx},{size}")
            # ─────────────────────────────────────────────────────────

            coords = [(b['x'], b['y']) for b in balls]
            route_pts, _ = nearest_neighbor_tsp(coords)
            draw_detections(frame_bgr, balls, route_pts)
            push_frame(frame_bgr)

            frame_count += 1
            if frame_count % 10 == 0:
                fps = 10 / (time.time() - t0)
                t0 = time.time()
                print(f"Frame {frame_count}: FPS={fps:.1f}  balls={len(balls)}",
                      flush=True)
                for i, b in enumerate(balls, 1):
                    print(f"  Ball {i}: ({b['x']},{b['y']}) "
                          f"src={b['source']} conf={b['conf']:.2f}", flush=True)

            if not no_display:
                cv2.imshow("Tennis Bot", frame_bgr)
                if cv2.waitKey(1) & 0xFF in (ord('q'), 27):
                    break

            if test_mode:
                print("Test mode: один кадр обработан.", flush=True)
                break

    except KeyboardInterrupt:
        print("\nОстановлено.")
    finally:
        cam.close()
        if not no_display:
            cv2.destroyAllWindows()


# ─────────────────────────── CAMERA ─────────────────────────────────

class CameraStream:
    def __init__(self, width=FRAME_W, height=FRAME_H):
        self.cam = Picamera2()
        cfg = self.cam.create_preview_configuration(
            main={"size": (width, height), "format": "RGB888"}
        )
        self.cam.configure(cfg)
        self.cam.start()
        time.sleep(1.5)
        print(f"Камера: {width}x{height} RGB888", flush=True)

    def read(self):
        return self.cam.capture_array()

    def close(self):
        self.cam.stop()
        self.cam.close()


# ─────────────────────────── ENTRY POINT ────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-display", action="store_true")
    parser.add_argument("--test",       action="store_true")
    parser.add_argument("--port",       type=int, default=8080)
    args = parser.parse_args()
    run(no_display=args.no_display, test_mode=args.test, stream_port=args.port)
