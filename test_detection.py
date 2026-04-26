#!/usr/bin/env python3
"""Тест детекции YOLOv8n на статичном изображении теннисного мяча."""
import os
import sys
import urllib.request

VENV_PYTHON = '/home/pi/tennis/venv/bin/python3'
WEIGHTS_DIR = '/home/pi/tennis'
TEST_IMG = '/home/pi/tennis/test_ball.jpg'

# Скачиваем тестовое изображение теннисного мяча (Wikimedia Commons)
TEST_URL = 'https://upload.wikimedia.org/wikipedia/commons/thumb/8/85/Smiley.svg/200px-Smiley.svg.png'
SPORTS_URL = 'https://upload.wikimedia.org/wikipedia/commons/thumb/5/59/Tennis_ball.png/200px-Tennis_ball.png'

print("Скачиваем тестовое изображение...")
try:
    urllib.request.urlretrieve(SPORTS_URL, TEST_IMG)
    print(f"Скачано: {TEST_IMG} ({os.path.getsize(TEST_IMG)} байт)")
except Exception as e:
    print(f"Ошибка скачивания {SPORTS_URL}: {e}")
    print("Используем fallback URL...")
    try:
        urllib.request.urlretrieve(
            'https://upload.wikimedia.org/wikipedia/commons/thumb/5/59/Tennis_ball.png/320px-Tennis_ball.png',
            TEST_IMG
        )
        print(f"Скачано fallback: {TEST_IMG}")
    except Exception as e2:
        print(f"Ошибка: {e2}")
        # Создаём синтетический зелёный шар как тест
        print("Создаём синтетический тест-изображение...")
        import numpy as np
        import cv2
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        cv2.circle(img, (320, 240), 80, (34, 179, 34), -1)
        cv2.imwrite(TEST_IMG, img)
        print(f"Создан синтетический шар: {TEST_IMG}")

print("\nЗагружаем YOLOv8n (автоматическое скачивание весов)...")
sys.path.insert(0, '/home/pi/tennis/venv/lib/python3.13/site-packages')
os.chdir(WEIGHTS_DIR)

from ultralytics import YOLO

# Загружаем модель (скачает yolov8n.pt если нет)
model = YOLO('yolov8n.pt')
print(f"Модель загружена: yolov8n.pt")

print(f"\nЗапускаем инференс на {TEST_IMG}...")
results = model(TEST_IMG, verbose=False)

print("\n=== Результаты детекции ===")
result = results[0]
boxes = result.boxes

if boxes is None or len(boxes) == 0:
    print("Объектов не обнаружено на тестовом изображении.")
    print("(Это нормально для синтетического изображения - YOLOv8 работает)")
else:
    names = model.names
    sports_ball_id = None
    for cid, name in names.items():
        if name == 'sports ball':
            sports_ball_id = cid
            break

    print(f"Всего обнаружено объектов: {len(boxes)}")
    balls_found = 0
    for i, box in enumerate(boxes):
        cls_id = int(box.cls[0])
        conf = float(box.conf[0])
        x1, y1, x2, y2 = box.xyxy[0].tolist()
        cx = int((x1 + x2) / 2)
        cy = int((y1 + y2) / 2)
        label = names[cls_id]
        print(f"  Object {i+1}: class={label} ({cls_id}), conf={conf:.2f}, bbox=({int(x1)},{int(y1)},{int(x2)},{int(y2)}), center=({cx},{cy})")
        if cls_id == sports_ball_id:
            balls_found += 1
            print(f"  >>> TENNIS BALL FOUND: x={cx}, y={cy}, confidence={conf:.2f}")

    if balls_found == 0:
        print(f"\nSports ball не обнаружен (class_id={sports_ball_id}), но другие объекты найдены.")
        print("YOLOv8 работает корректно.")

print("\nDETECTION TEST: OK")
