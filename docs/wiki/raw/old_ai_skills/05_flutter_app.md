# 05 Flutter App

## Role

The Flutter app provides:

- WebSocket connection to ESP32.
- Manual joystick control.
- Court and robot map.
- Zone selection.
- Calibration pose and heading.
- Telemetry display.
- Developer terminal/log view.

## Geometry Rule

The app must visually distinguish:

- Playable court.
- Robot working area.
- Diagonal UWB anchors at working-area corners.

The app should not treat playable court dimensions as the whole robot coordinate system.

## Anchor Rule

First implementation:

- Anchor positions are fixed in app layout.
- User does not drag anchors.
- User places physical anchors as shown.

## Calibration Rule

Calibrate sends at minimum:

- `robot_x`
- `robot_y`
- `robot_heading`

Coordinates must be in robot working area meters, not raw screen pixels.

The app should make it clear whether calibration succeeded or failed based on ESP32 response.

## Current App Risk

The main screen is a large monolithic file:

- `tennis_app/lib/screens/court_zones_screen.dart`

Before product/demo polish, split or at least isolate:

- Geometry transforms.
- Calibration UI.
- Zone selection.
- Joystick controls.
- Telemetry panel.
- Developer tools.
