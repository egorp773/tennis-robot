#!/usr/bin/env python3
"""Тест камеры через picamera2 на Raspberry Pi."""
import time
from picamera2 import Picamera2
import numpy as np

print("Инициализация камеры...")
cam = Picamera2()

# Конфигурация для захвата кадра
config = cam.create_still_configuration(
    main={"size": (640, 480), "format": "RGB888"}
)
cam.configure(config)

print("Запуск камеры...")
cam.start()
time.sleep(2)  # Прогрев камеры

print("Захват кадра...")
frame = cam.capture_array()
cam.stop()
cam.close()

print(f"Кадр захвачен: shape={frame.shape}, dtype={frame.dtype}")
print(f"Min pixel: {frame.min()}, Max pixel: {frame.max()}")

# Сохраняем через numpy (без OpenCV для чистого теста)
import struct, zlib

def save_png(filename, rgb_array):
    """Минимальное сохранение PNG без зависимостей."""
    h, w, c = rgb_array.shape
    raw_rows = []
    for row in rgb_array:
        raw_rows.append(b'\x00' + row.tobytes())
    raw_data = b''.join(raw_rows)
    compressed = zlib.compress(raw_data)

    def chunk(type_bytes, data):
        length = struct.pack('>I', len(data))
        crc = struct.pack('>I', zlib.crc32(type_bytes + data) & 0xffffffff)
        return length + type_bytes + data + crc

    ihdr_data = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', ihdr_data)
    png += chunk(b'IDAT', compressed)
    png += chunk(b'IEND', b'')

    with open(filename, 'wb') as f:
        f.write(png)

save_png('/tmp/test_frame.png', frame)

import os
size = os.path.getsize('/tmp/test_frame.png')
print(f"Сохранено /tmp/test_frame.png ({size} байт)")

if size > 1000:
    print("CAMERA TEST: OK")
else:
    print("CAMERA TEST: FAILED - файл слишком маленький")
