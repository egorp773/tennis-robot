# 01 Project Overview

## Purpose

This project builds a tennis-ball collection robot with:

- Flutter app for control and telemetry.
- ESP32 robot controller for motors, collector, WebSocket, UWB, and state machine.
- Raspberry Pi camera system for tennis-ball detection.
- Two ESP32 UWB anchors for localization.

## Main Components

- `main.py`: Raspberry Pi camera, YOLO detection, MJPEG stream, UART commands to ESP32.
- `esp32_firmware/tennis_robot/tennis_robot.ino`: robot firmware.
- `esp32_firmware/anchor_L/` and `esp32_firmware/anchor_R/`: UWB anchors.
- `tennis_app/lib/`: Flutter app.
- `calibration/`: UWB measurement and analysis scripts.
- `training/`: YOLO training and deployment scripts.

## Current Status

The app can manually control the robot. Autonomous mode exists in code but is not safe for unrestricted real-court operation yet.

The main blockers are:

- Shared geometry is not fully aligned across app, ESP32, and calibration scripts.
- UWB still needs two-candidate branch tracking.
- Safety stops need to be stricter.
- Real-court camera/YOLO reliability is unknown.

## Work Rule

Before code changes, check:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- `docs/ai_skills/*.md`
