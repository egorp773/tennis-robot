# CLAUDE.md

This file provides guidance to Claude Code when working with this repository.

## Required Reading

Before non-trivial changes, read:

- `MEMORY.md`
- `DECISIONS.md`
- `AUTONOMY_ROADMAP.md`
- `IMPLEMENTATION_STATUS.md`
- Relevant `docs/ai_skills/*/SKILL.md`

For test history, read:

- `TEST_LOG.md`

## Hardware

- Raspberry Pi 4: `<TENNIS_PI_HOST>`, user `pi`.
- Camera: IMX708 Wide NoIR via `picamera2`.
- ESP32 robot: creates Wi-Fi AP `TennisRobot / <configured password>`, WebSocket port `81`.
- ESP32 robot receives Pi UART commands and controls hoverboard motors.
- Two ESP32 UWB anchors use DW1000.

## Core Geometry Rule

Do not assume the playable tennis court and robot working area are the same rectangle.

Playable court:

- Standard tennis court, `23.77 x 10.97 m`.
- Used for lines, net, service boxes, and court zones.

Robot working area:

- Larger rectangle around the playable court.
- Current fixed first implementation: court plus `3.00 m` margin on each side.
- Current size: `29.77 x 16.97 m`.
- Playable court origin inside working area: `(3.00, 3.00)`.
- Used for robot localization, boundary checks, outside-court collection, and UWB anchors.

UWB anchors:

- Anchor L: `(0.00, 0.00)`, one diagonal corner of robot working area.
- Anchor R: `(29.77, 16.97)`, opposite diagonal corner of robot working area.
- These are not net posts.
- These are not draggable in the first app implementation.

## UWB Rule

The project currently uses only two UWB anchors.

- Two anchors produce two possible UWB candidate positions.
- Calibration selects the initial candidate closest to the app-provided robot pose.
- Runtime tracking selects the candidate closest to the last accepted pose.
- Impossible jumps must be rejected or trigger STOP.
- Repeated rejected UWB samples must trigger STOP.
- Leaving the robot working area must trigger STOP.
- AUTO must not start without valid calibration and heading.

## Running Pi Detection

On Pi:

```bash
ssh pi@<TENNIS_PI_HOST>
source /home/pi/tennis/venv/bin/activate

python3 main.py --no-display
python3 main.py --test
python3 main.py
```

From Windows via Paramiko helpers:

```bash
python3 run_cmd.py "python3 /home/pi/tennis/main.py --test"
python3 check_deps.py
```

Local tests:

```bash
python3 test_tsp.py
python3 test_detection.py
python3 test_camera.py
```

## ESP32 Firmware

Compile examples:

```bash
./arduino-cli-bin/arduino-cli.exe lib install "MPU6050_light"

./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/tennis_robot/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/anchor_L/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/anchor_R/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/tennis_robot_cal/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/manual_control/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/collector_test/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/camera_only_collection/
```

Current firmware directories:

- `esp32_firmware/tennis_robot/`: full integrated robot firmware.
- `esp32_firmware/anchor_L/`: UWB anchor L.
- `esp32_firmware/anchor_R/`: UWB anchor R.
- `esp32_firmware/tennis_robot_cal/`: calibration-oriented robot/tag firmware.
- `esp32_firmware/manual_control/`: isolated manual-control test firmware.
- `esp32_firmware/collector_test/`: isolated collector relay/sensor/counter test firmware.
- `esp32_firmware/camera_only_collection/`: Pi camera guided collection without UWB.
- `esp32_firmware/1000__working/`: preserved known-working snapshot.

Firmware variants should remain explicit. Do not force every hardware test through the full autonomous firmware.

Required or planned test variants:

- UWB calibration: raw ranges, candidate positions, minimal or disabled motors.
- Manual motor test: motor commands without autonomous navigation.
- Collector test: implemented in `collector_test`.
- Camera-only collection: implemented in `camera_only_collection`.
- Safety test: stale UWB, jump rejection, out-of-bounds, Pi timeout, STOP reason checks.
- Demo mode: one-zone, low speed, conservative timeouts.

## Flutter App

```bash
cd tennis_app

flutter pub get
flutter analyze lib
flutter run
flutter build apk
flutter run -d chrome
```

Known local blocker from previous status:

- Flutter/Windows commands had hung because of stale Flutter lock files and invalid proxy variables.
- See `IMPLEMENTATION_STATUS.md` before trusting local Flutter verification.

## UWB Calibration

```bash
cd calibration

python measure.py
python analyze.py
python monitor.py
```

Use `calibration/README.md` as the current procedure. Record physical test results in `TEST_LOG.md`.

## Pi Vision Architecture

`main.py` owns:

- `picamera2` camera capture;
- YOLOv8 detection;
- MJPEG stream at port `8080`;
- UART commands to ESP32.

Pi to ESP32 UART:

- `TRACK:dx,size`
- `COLLECT`
- `SEARCH`
- `RETURN`

The current default model is COCO `yolov8n.pt` class `sports ball`. Real-court validation is still required.

## App Robot Protocol

App to ESP32:

- `M,left,right`
- `STOP`
- `ATTACHMENT_ON`
- `ATTACHMENT_OFF`
- `CALIBRATE:heading:x:y`
- `ZONES:x1,y1;x2,y2`
- `AUTO_START`
- `AUTO_STOP`
- `SET_HOME:x,y`
- `SET_BALLS:n`

Coordinates for calibration, zones, and home are robot working-area meters.

ESP32 to app:

- `STATE,<state>`
- `POS,x,y`
- `BALLS_COUNT,n`
- `ANCHOR,L,online/offline`
- `ANCHOR,R,online/offline`
- `BAT_PCT,n`
- `CALIBRATED,x,y`
- `ERROR,<reason>`
- `WARN,<reason>`

## Safety

AUTO must be conservative until real tests prove otherwise.

Before unrestricted autonomous movement:

- UWB calibration must be measured and logged.
- STOP on UWB failure must be verified.
- STOP on Pi failure must be verified.
- One-zone route must be constrained.
- Physical emergency cutoff must be available.

Do not overclaim readiness. Current status is prototype, not product.
