---
name: raspberry-pi-vision
description: Use when working on Raspberry Pi camera capture, YOLO tennis-ball detection, MJPEG streaming, UART commands from Pi to ESP32, model deployment, or vision testing.
---

# Raspberry Pi Vision

## Main File

- `main.py`

## Responsibilities

The Pi side owns:

- `picamera2` camera capture.
- YOLO inference.
- MJPEG stream.
- Annotated frames.
- UART commands to ESP32.

## UART Commands To ESP32

- `TRACK:dx,size`: ball visible, `dx` is horizontal pixel offset from frame center.
- `COLLECT`: ball is close/large enough.
- `SEARCH`: no ball visible.
- `RETURN`: basket/container full or return requested.

## Current Vision Risk

The current default model is COCO `yolov8n.pt` class `sports ball`.

Expected issue:

- It may miss tennis balls or false-positive on real court backgrounds.
- Real-court testing is required.
- A custom `best.pt` is likely needed after navigation is stable.

## Test Requirements

Test on real court with:

- Sun.
- Shadows.
- Lines.
- Net.
- Balls at multiple distances.
- Multiple balls.
- Similar colored background objects.

Log:

- FPS.
- Detection count.
- Confidence.
- Bounding boxes.
- UART messages sent.
