# AGENTS.md

This file gives Codex project-specific instructions for `C:\robot\tennis`.

## Required Reading Before Work

Before any non-trivial change, read:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- `IMPLEMENTATION_STATUS.md`
- Relevant `docs/ai_skills/*/SKILL.md`

For test history, also read:

- `TEST_LOG.md`

## Project Rule

Do not assume the playable tennis court and robot working area are the same rectangle.

- Playable court: standard tennis court, used for court lines, net, service boxes, and game zones.
- Robot working area: larger rectangle around the playable court where the robot may drive and collect balls.
- Two UWB anchors are opposite diagonal corners of the robot working area.
- The user physically places anchors according to the app layout.
- The app does not need draggable anchors in the first implementation.

## UWB Rule

The project currently uses only two UWB anchors.

- Two anchors produce two possible UWB candidate positions.
- Calibration selects the initial candidate closest to the app-provided robot pose.
- Runtime tracking selects the candidate closest to the last accepted pose.
- Impossible jumps must be rejected or trigger STOP.
- Repeated rejected UWB samples must trigger STOP.
- Leaving the robot working area must trigger STOP.
- AUTO must not start without valid calibration and heading.

## Editing Rule

Do not change code unless explicitly asked.

When changing architecture, update memory files:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- `IMPLEMENTATION_STATUS.md`
- Relevant `docs/ai_skills/*/SKILL.md`

When running physical tests, append results to:

- `TEST_LOG.md`

## Current Hardware Summary

- Raspberry Pi 4 at `<TENNIS_PI_HOST>`, user `pi`.
- ESP32 robot creates Wi-Fi AP `TennisRobot / <configured password>`, WebSocket port `81`.
- ESP32 robot receives Pi UART commands and controls hoverboard motors.
- Two ESP32 UWB anchors use DW1000.
- Camera is IMX708 Wide NoIR via `picamera2`.

## Main Entry Points

- Pi detection: `main.py`
- ESP32 robot firmware: `esp32_firmware/tennis_robot/tennis_robot.ino`
- UWB anchors: `esp32_firmware/anchor_L/`, `esp32_firmware/anchor_R/`
- Flutter app: `tennis_app/lib/`
- Calibration scripts: `calibration/`
- Training scripts: `training/`
