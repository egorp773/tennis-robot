---
name: esp32-firmware
description: Use when working on ESP32 robot firmware, state machine, WebSocket commands, UART from Raspberry Pi, hoverboard motor commands, UWB integration, collector relay, or telemetry.
---

# ESP32 Firmware

## Main File

- `esp32_firmware/tennis_robot/tennis_robot.ino`

## Firmware Variants

Keep explicit firmware variants or compile-time test modes for isolated hardware tests.

Current directories:

- `esp32_firmware/tennis_robot/`: full integrated robot firmware.
- `esp32_firmware/anchor_L/`: UWB anchor L.
- `esp32_firmware/anchor_R/`: UWB anchor R.
- `esp32_firmware/tennis_robot_cal/`: UWB calibration/range-focused firmware.
- `esp32_firmware/manual_control/`: isolated manual-control firmware.
- `esp32_firmware/collector_test/`: collector relay/sensor/counter test firmware.
- `esp32_firmware/camera_only_collection/`: Pi camera guided collection without UWB.
- `esp32_firmware/1000__working/`: preserved known-working snapshot.

Required/planned variants:

- UWB calibration: raw ranges, candidate positions, no autonomous movement.
- Manual motor test: motor commands without AUTO.
- Collector test: implemented in `collector_test`.
- Camera-only collection: implemented in `camera_only_collection`.
- Safety test: stale UWB, jump rejection, out-of-bounds, Pi timeout, STOP reason checks.
- Demo mode: one-zone, low-speed, strict timeouts.

Do not make a test require the full autonomous firmware unless the test is specifically about full integration.

## Responsibilities

The ESP32 robot firmware owns:

- Wi-Fi AP and WebSocket server.
- Manual joystick command handling.
- AUTO state machine.
- UART1 communication with Raspberry Pi.
- UART2 hoverboard motor protocol.
- UWB tag ranging and position tracking.
- MPU6050 heading.
- Collector relay.
- Basket and ball sensors.
- Telemetry to app.
- Safety stops.

## Current AUTO States

- `ST_MANUAL`: phone joystick control.
- `ST_AUTO`: autonomous search, tracking, collecting, returning.
- `ST_UNLOADING`: wait for basket removal/return.

AUTO sub-states:

- `AS_SEARCH`
- `AS_TRACKING`
- `AS_COLLECTING`
- `AS_RETURNING`

## Required Architecture Changes

Current firmware must evolve from single hidden two-anchor solution to:

- Two UWB candidates.
- Calibration candidate selection.
- Runtime candidate tracking.
- Jump rejection.
- Working-area boundary check.
- Clear safety reason reporting.

## Firmware Safety Priority

Motor commands must always fail safe. If localization, heading, Pi link, or boundary state is invalid, stop motors before doing anything else.

## Do Not Assume

Do not assume:

- Playable court dimensions equal robot working area dimensions.
- Anchors are net posts.
- Anchors have equal `X`.
- Two-anchor UWB is unambiguous.
