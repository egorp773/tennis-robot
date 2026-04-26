---
name: app-robot-protocol
description: Use when changing WebSocket commands or telemetry between the Flutter app and ESP32, including calibration payloads, AUTO commands, telemetry parsing, safety errors, and terminal logging.
---

# App Robot Protocol

## Transport

Flutter app connects to ESP32 WebSocket:

- Host: `192.168.4.1`
- Port: `81`
- Path used by app: `/ws`

## Existing Command Categories

App to ESP32:

- Manual movement: `M,left,right`
- Stop: `STOP`
- Auto: `AUTO_START`, `AUTO_STOP`
- Collector: `ATTACHMENT_ON`, `ATTACHMENT_OFF`
- Calibration: currently `CALIBRATE...`
- Zones: `ZONES:x1,y1;x2,y2`
- Config: `SET_HOME:x,y`, `SET_BALLS:n`, `SET_SPEED:n`
- Debug/raw commands from developer terminal.

ESP32 to app:

- `STATE,<state>`
- `POS,x,y`
- `BALLS_COUNT,n`
- `ANCHOR,L,online/offline`
- `ANCHOR,R,online/offline`
- `BAT_PCT,n`
- `ERROR,<reason>`
- `WARN,<reason>`

## Required Calibration Payload

Calibration should explicitly include:

- `robot_x`
- `robot_y`
- `robot_heading`

Coordinates must be robot working area meters.

ESP32 should respond with success/failure:

- `CALIBRATED,x,y`
- `ERROR,CALIBRATION_MISMATCH`
- `ERROR,HEADING_INVALID`
- `ERROR,UWB_STALE`

## Protocol Rule

Any new command that can move motors must have:

- Preconditions.
- Failure response.
- Terminal log entry.
- Safety behavior on timeout or invalid state.
