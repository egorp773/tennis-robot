# TEST_LOG.md

Append every physical or integration test here. Keep entries factual.

## Quick Entry Template

Use this for normal short test reports sent in chat.

```markdown
## YYYY-MM-DD HH:MM - Test Name

- Type: manual control / UWB anchors / camera / Pi UART / Flutter app / AUTO / collector / other
- Firmware/app version or mode:
- Goal:
- What was done:
- Result:
- Problems:
- Next step:
```

Examples of valid short entries:

- manual control test: joystick movement, stop button, collector relay;
- UWB anchors test: anchor online/offline, ranges, position stability;
- camera test: camera starts, FPS, detections, false positives;
- Pi/ESP32 UART test: `TRACK`, `SEARCH`, `COLLECT`, `RETURN` messages;
- Flutter app test: connection, calibration command, telemetry parsing;
- AUTO safety test: rejected start, UWB stale stop, out-of-bounds stop.

If a test involves real robot movement, UWB calibration, camera detection on court, or AUTO behavior, use the detailed template below or add the missing details after the quick entry.

## Test Template

```markdown
## YYYY-MM-DD - Short Test Name

- Date:
- Goal:
- Operator:
- Location:
- Weather/light:
- Hardware:
- Firmware/app versions:

### Anchor Placement

- Anchor L physical position:
- Anchor R physical position:
- Robot working area size:
- Playable court offset inside working area:
- Notes:

### Calibration Pose

- App robot_x:
- App robot_y:
- App robot_heading:
- ESP32 accepted candidate:
- Calibration mismatch:
- Calibration result:

### UWB Result

- Anchor L online:
- Anchor R online:
- Range stability:
- Position RMS or observed error:
- Rejected samples:
- Boundary events:

### Pi Status

- Pi running command:
- UART status:
- FPS:
- Detection count:
- Errors:

### Camera Status

- Camera mode:
- YOLO model:
- Lighting:
- False positives:
- Missed balls:

### Robot Result

- AUTO/manual:
- Zone target:
- Movement result:
- Safety stop triggered:
- Final state:

### Failure Reason

- UWB:
- Camera:
- Pi link:
- ESP32 logic:
- Motors:
- App:
- Unknown:

### Next Fix

- Immediate fix:
- Follow-up test:
```

## Entries

No physical test entries recorded yet.
