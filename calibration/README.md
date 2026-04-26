# UWB Calibration

This calibration flow is for the current two-anchor diagonal working-area model.

## Geometry

- Playable court: `23.77 x 10.97 m`.
- Robot working area: playable court plus `3.00 m` margin on each side.
- Working area size: `29.77 x 16.97 m`.
- Playable court origin inside the working area: `(3.00, 3.00)`.
- Anchor L: `(0.00, 0.00)`, one diagonal corner of the working area.
- Anchor R: `(29.77, 16.97)`, opposite diagonal corner of the working area.

The app, ESP32 robot firmware, anchor comments, and calibration scripts must use these same coordinates until configurable geometry is implemented.

## What Calibration Measures

DW1000 ranging needs antenna-delay calibration and a real-world accuracy check. With only two anchors, there are always two possible position candidates, so raw `RANGE,L/R` values are more important than the displayed `POS` during antenna-delay measurement.

The main robot firmware resolves the ambiguity through app calibration:

- The user places the robot in the real working area.
- The user places the robot icon at the same point in the app.
- The user sets heading.
- The app sends `CALIBRATE:heading:x:y`.
- ESP32 chooses the UWB candidate closest to the app pose.

## Required Equipment

- Robot ESP32 with robot or calibration firmware.
- Two UWB anchor ESP32 boards flashed as `anchor_L` and `anchor_R`.
- Tape measure.
- Tape markers for reference points.
- Windows laptop connected to `TennisRobot / <configured password>`.

## Firmware

From repository root:

```bash
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/anchor_L/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/anchor_R/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/tennis_robot/
./arduino-cli-bin/arduino-cli.exe compile --fqbn esp32:esp32:esp32 esp32_firmware/tennis_robot_cal/
```

Use `tennis_robot_cal` when you want continuous raw `RANGE` telemetry without motor control. Use `tennis_robot` when you want to test the real app calibration flow.

## Measurement Procedure

1. Place Anchor L and Anchor R at the two diagonal working-area corners shown in the app.
2. Keep anchors at similar height to the robot UWB tag.
3. Verify line-of-sight and remove metal objects near anchors when possible.
4. Connect the laptop to Wi-Fi `TennisRobot / <configured password>`.
5. Run:

```bash
cd calibration
python measure.py
```

6. Move the robot center to each prompted reference point.
7. Wait for the script to collect samples.
8. Save `measurements.json`.
9. Run:

```bash
python analyze.py
```

Important:

- Existing `measurements.json` may be an old/synthetic/sample file.
- Treat a calibration file as valid only if its `working_area`, anchor coordinates, date, and `TEST_LOG.md` entry match the current physical setup.

## Acceptance Gates

- For cautious first autonomous movement: target `RMS 2D < 0.50 m`.
- For a stable demo: target `RMS 2D < 0.30 m`.
- If `RMS 2D >= 1.00 m`, do not run free autonomous navigation.

## Logging

After each physical test, record results in `../TEST_LOG.md`:

- anchor placement;
- working area size;
- calibration pose;
- UWB error;
- Pi/camera state;
- safety stop reason if any;
- next fix.
