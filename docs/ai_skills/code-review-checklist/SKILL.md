---
name: code-review-checklist
description: Use when reviewing changes to this project for correctness, safety, coordinate consistency, protocol compatibility, autonomous behavior, testing gaps, or demo readiness.
---

# Code Review Checklist

## Geometry

Check:

- Playable court and robot working area are not confused.
- App, ESP32, and calibration scripts use the same coordinate orientation.
- Targets are checked against robot working area boundary.
- Anchor coordinates match diagonal working-area model.

## UWB

Check:

- Trilateration returns two candidates.
- Calibration selects nearest candidate to app pose.
- Runtime selects candidate nearest previous accepted pose.
- Impossible jumps are rejected.
- Repeated rejects cause STOP.
- AUTO is blocked without calibration and heading.

## Safety

Check:

- Motors stop on UWB lost/stale.
- Motors stop on out-of-bounds.
- Motors stop on Pi timeout during camera-guided movement.
- Safety reasons are sent to app/log.
- Timeouts are conservative for first tests.

## Protocol

Check:

- Flutter and ESP32 agree on WebSocket commands.
- Pi and ESP32 agree on UART commands.
- New commands have failure responses.
- Terminal/logging captures important TX/RX.

## Vision

Check:

- YOLO assumptions are explicit.
- Real-court test gaps are documented.
- Pi FPS and UART behavior are considered.

## Tests

Check:

- Physical tests are logged in `TEST_LOG.md`.
- New behavior has at least a dry-run or unit-level verification where possible.
- Demo changes can fail safely.
