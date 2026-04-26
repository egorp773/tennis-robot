# 04 ESP32 Safety

## Required AUTO Preconditions

ESP32 must not start AUTO unless:

- `calibration_valid=true`
- Robot heading is valid.
- UWB has fresh usable data.
- Accepted pose is inside robot working area.
- At least one valid target zone exists or the selected demo mode has a safe fallback.

## Required STOP Conditions

ESP32 must stop motors if:

- Both UWB anchors are lost.
- UWB range data is stale.
- Position jumps impossibly.
- Several UWB samples are rejected consecutively.
- Accepted pose leaves robot working area.
- Robot approaches working-area boundary too closely and cannot slow safely.
- Pi UART is stale while camera-guided movement is active.
- RETURN or navigation gets stuck beyond timeout.

## Messages To App

Safety stops should send clear WebSocket messages, for example:

- `ERROR,UWB_LOST`
- `ERROR,UWB_STALE`
- `ERROR,UWB_JUMP`
- `ERROR,UWB_BRANCH`
- `ERROR,OUT_OF_BOUNDS`
- `ERROR,NOT_CALIBRATED`
- `ERROR,HEADING_INVALID`
- `WARN,PI_TIMEOUT`

## Initial Thresholds

Use conservative values first:

- Calibration mismatch: `0.50-1.00 m`
- UWB stale timeout: `2-5 s`
- Pi timeout in autonomous movement: `3-5 s`
- Consecutive UWB rejects before STOP: `3-5`
- Boundary slow margin: tune after measuring robot stopping distance.

All thresholds must be tuned from `TEST_LOG.md`.
