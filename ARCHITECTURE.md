# Tennis Robot Architecture

Last updated: 2026-04-26.

This document is the current architecture summary. Older notes that describe anchors at net posts or a single court-sized robot boundary are obsolete.

## System Overview

The project is a tennis-ball collection robot prototype with four main parts:

- Flutter app: manual control, robot map, zone selection, calibration, telemetry, developer terminal.
- ESP32 robot controller: Wi-Fi AP, WebSocket server, hoverboard motor output, UWB tag tracking, state machine, collector relay, safety stops.
- Raspberry Pi 4 vision: IMX708 camera, YOLO ball detection, MJPEG stream, UART commands to ESP32.
- Two ESP32 UWB anchors: DW1000 anchor firmware at fixed physical positions.

## Current Source Of Truth

Read these before changing behavior:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- `IMPLEMENTATION_STATUS.md`
- Relevant `docs/ai_skills/*/SKILL.md`
- `TEST_LOG.md` for real test history

## Geometry

The project uses two different rectangles.

Playable court:

- Standard tennis court.
- Size: `23.77 x 10.97 m`.
- Used for court lines, net, service boxes, gameplay zones, and visual court drawing.

Robot working area:

- Larger rectangle around the playable court.
- Used for robot localization, movement boundary, outside-court collection, UWB anchor placement, and safety stops.
- Current fixed first implementation: playable court plus `3.00 m` margin on each side.
- Current working-area size: `29.77 x 16.97 m`.
- Playable court origin inside working area: `(3.00, 3.00)`.

Two UWB anchors:

- Anchor L: `(0.00, 0.00)`, one diagonal corner of the robot working area.
- Anchor R: `(29.77, 16.97)`, opposite diagonal corner of the robot working area.
- These are not net posts and not necessarily playable court corners.
- The app shows this fixed layout; the user physically places anchors according to the app.
- The first implementation does not need draggable anchors.

## UWB Positioning

The prototype currently uses only two UWB anchors.

Two anchors produce two possible circle-intersection positions:

- `candidate_A`
- `candidate_B`

Required behavior:

1. Compute both candidates from the two anchor ranges.
2. During calibration, compare both candidates with the app-provided robot pose.
3. Select the candidate closest to `robot_x,robot_y`.
4. Store the accepted pose and branch/side.
5. During runtime, choose the candidate closest to the last accepted pose.
6. Reject impossible jumps.
7. Stop after repeated rejected UWB samples.
8. Stop if the accepted pose exits the robot working area.

AUTO must not start without:

- valid calibration;
- valid heading;
- fresh accepted UWB pose;
- target inside robot working area.

## App To Robot Protocol

Flutter connects to ESP32 WebSocket:

- ESP32 AP: `TennisRobot / <configured password>`
- ESP32 address: `192.168.4.1`
- WebSocket port: `81`

Important app commands:

- `M,left,right`: manual movement, each side `-100..100`.
- `STOP`: immediate motor stop.
- `ATTACHMENT_ON`, `ATTACHMENT_OFF`: collector relay.
- `CALIBRATE:heading:x:y`: robot pose calibration in robot working-area meters.
- `ZONES:x1,y1;x2,y2;...`: target zones in robot working-area meters.
- `AUTO_START`, `AUTO_STOP`: autonomous mode control.
- `SET_HOME:x,y`: home/unload point in robot working-area meters.
- `SET_BALLS:n`: max ball count.

Important ESP32 telemetry:

- `STATE,<state>`
- `POS,x,y`
- `BALLS_COUNT,n`
- `ANCHOR,L,online/offline`
- `ANCHOR,R,online/offline`
- `BAT_PCT,n`
- `CALIBRATED,x,y`
- `ERROR,<reason>`
- `WARN,<reason>`

## Pi To ESP32 UART

Pi UART commands:

- `TRACK:dx,size`: ball is visible; `dx` is horizontal pixel offset from frame center, `size` is bounding-box area.
- `COLLECT`: ball is close enough by current vision heuristic.
- `SEARCH`: no ball visible.
- `RETURN`: basket full or return requested.

ESP32 must stop or fall back safely if Pi UART becomes stale while autonomous camera-guided movement depends on Pi.

## Main Files

Root:

- `main.py`: Raspberry Pi camera, YOLO detection, MJPEG stream, UART commands.
- `ssh_helper.py`: Paramiko helper for Pi commands.
- `run_cmd.py`: CLI wrapper around SSH command execution.
- `check_deps.py`: Pi dependency check helper.

ESP32:

- `esp32_firmware/tennis_robot/tennis_robot.ino`: main integrated robot firmware.
- `esp32_firmware/anchor_L/anchor_L.ino`: UWB anchor L.
- `esp32_firmware/anchor_R/anchor_R.ino`: UWB anchor R.
- `esp32_firmware/tennis_robot_cal/tennis_robot_cal.ino`: calibration-oriented robot/tag firmware.
- `esp32_firmware/manual_control/manual_control.ino`: isolated manual-control test firmware.
- `esp32_firmware/collector_test/collector_test.ino`: isolated collector relay/sensor/counter test firmware.
- `esp32_firmware/camera_only_collection/camera_only_collection.ino`: Pi camera guided collection without UWB.
- `esp32_firmware/1000__working/1000__working.ino`: preserved working firmware snapshot.

Flutter:

- `tennis_app/lib/main.dart`: app entry point.
- `tennis_app/lib/app.dart`: root widget.
- `tennis_app/lib/services/wifi_connection.dart`: WebSocket client, telemetry parsing, Riverpod provider.
- `tennis_app/lib/screens/court_zones_screen.dart`: main screen.
- `tennis_app/lib/painters/court_painter.dart`: robot working area, playable court, anchors, robot, and zones.
- `tennis_app/lib/models/court_models.dart`: UI/data models.

Calibration:

- `calibration/measure.py`: collect UWB measurements at known working-area points.
- `calibration/analyze.py`: analyze measurement error and calibration quality.
- `calibration/monitor.py`: live position/range monitoring.
- `calibration/README.md`: current UWB calibration procedure.

Training:

- `training/train.py`: local YOLO training.
- `training/train_colab.py`: Colab training helper.
- `training/upload_model.py`: upload trained model to Pi.

## Firmware Variants For Testing

The project should keep separate firmware variants or compile-time test modes for isolated hardware tests. The goal is to avoid testing every subsystem through the full autonomous firmware.

Current variants:

- `tennis_robot`: full integrated robot firmware.
- `anchor_L`: UWB anchor L.
- `anchor_R`: UWB anchor R.
- `tennis_robot_cal`: UWB calibration/range-focused robot firmware.
- `manual_control`: isolated motor/manual-control firmware.
- `collector_test`: collector relay, basket sensor, ball sensor, and count firmware.
- `camera_only_collection`: Pi camera `TRACK/COLLECT/SEARCH` collection test without UWB.
- `1000__working`: preserved known-working snapshot.

Required or planned variants:

- UWB calibration firmware: raw ranges and candidate positions, minimal or disabled motor output.
- Manual motor test firmware: joystick/WebSocket or serial motor commands, no autonomous navigation.
- Collector test firmware: implemented in `collector_test`.
- Camera-only collection firmware: implemented in `camera_only_collection`.
- Safety test firmware or mode: inject stale UWB, out-of-bounds, jump rejection, Pi timeout, and verify STOP reasons.
- Demo firmware/mode: one-zone, low-speed, conservative timeouts, app-visible stop reasons.

Every firmware variant must state:

- purpose;
- hardware it is allowed to move;
- safety limits;
- expected serial/WebSocket commands;
- how to log results in `TEST_LOG.md`.

## Safety Rules

Motor output must fail safe.

Stop motors if:

- both UWB anchors are lost;
- range data is stale;
- accepted pose exits robot working area;
- UWB candidate jumps are physically impossible;
- repeated UWB samples are rejected;
- Pi UART is stale during camera-guided movement;
- AUTO target is outside robot working area;
- calibration or heading is invalid.

Before any real autonomous test:

- compile firmware;
- verify Flutter command format;
- calibrate UWB and record results in `TEST_LOG.md`;
- test STOP behavior;
- use one small zone and low speed;
- keep a physical emergency power cutoff available.

## Current Readiness

- Manual control: usable prototype.
- App telemetry: partial prototype.
- UWB localization: architecture updated, needs compile verification and physical calibration.
- Autonomous mode: not ready for unrestricted real-court use.
- Camera/YOLO: pipeline exists, real-court accuracy still needs validation.
- Demo readiness: possible only with constrained one-zone demo after safety and UWB validation.
