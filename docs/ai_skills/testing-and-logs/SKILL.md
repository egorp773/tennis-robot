---
name: testing-and-logs
description: Use when planning, running, or analyzing physical tests, UWB calibration tests, camera tests, autonomous zone tests, demo rehearsals, or updating TEST_LOG.md.
---

# Testing And Logs

## Test Log Rule

Every real test must be appended to `TEST_LOG.md`.

Do not rely on memory or chat history for physical test results.

Short chat-style test reports are acceptable. Convert them into `TEST_LOG.md` entries using the quick template when the user sends results like manual control test, UWB anchor test, camera test, or collector test.

## Minimum Data To Capture

For every test capture:

- Date and goal.
- Anchor placement.
- Robot working area size.
- Playable court offset inside working area.
- Calibration pose.
- UWB result.
- Pi status.
- Camera status.
- Robot result.
- Failure reason.
- Next fix.

For firmware-variant tests, also capture:

- firmware directory or mode;
- firmware version/comment if available;
- which hardware was allowed to move;
- whether UWB, Pi UART, collector, or motors were intentionally disabled.

## Useful Logs

ESP32:

- State/sub-state.
- UWB ranges.
- UWB candidates.
- Accepted pose.
- Rejection reason.
- Motor command.
- Safety stop reason.

Pi:

- FPS.
- Detection count.
- Detection confidence.
- UART TX/RX.
- Camera errors.

Flutter:

- WebSocket TX/RX.
- Calibration request/response.
- Anchor online/offline.
- Error/warning messages.

## Debugging Principle

After a failed run, classify the root cause as one of:

- UWB geometry/tracking.
- Calibration.
- Camera/detection.
- Pi/ESP32 link.
- ESP32 state machine.
- Motor control.
- App command/UI.
- Unknown.
