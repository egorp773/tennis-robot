---
name: project-overview
description: Use when starting work on this repository, explaining the system, orienting around files, or checking current project status before deeper changes.
---

# Project Overview

## Purpose

This project builds a tennis-ball collection robot with:

- Flutter app for control, calibration, zones, telemetry, and developer terminal.
- ESP32 robot controller for WebSocket, motor commands, collector relay, UWB, state machine, and safety.
- Raspberry Pi camera system for tennis-ball detection and UART commands to ESP32.
- Two ESP32 UWB anchors for robot localization.

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

## Core Architecture

There are two different areas:

- Playable court: standard tennis court with lines, net, service boxes, and game zones.
- Robot working area: larger rectangle around the court where robot may drive and collect balls outside court lines.

The playable court is inside the robot working area.

The two UWB anchors are diagonal corners of the robot working area, not necessarily corners of the playable court.

## Work Rule

Before code changes, check:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- Relevant `docs/ai_skills/*/SKILL.md`
