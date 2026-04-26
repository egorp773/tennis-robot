# MEMORY.md

This is the long-term project memory for Codex.

Related notes: [[DECISIONS]], [[AUTONOMY_ROADMAP]], [[TEST_LOG]], [[geometry-and-calibration]], [[uwb-two-anchor-diagonal]], [[esp32-safety]], [[autonomous-mode]].

## Current State

The project is a tennis-ball collection robot prototype.

Working parts:

- Flutter app can manually control the robot over ESP32 WebSocket.
- ESP32 robot firmware has manual mode, autonomous state machine, UWB reads, Pi UART protocol, hoverboard motor output, relay collector control, and telemetry.
- Raspberry Pi side has camera capture, YOLOv8 sports-ball detection, MJPEG stream, and UART commands to ESP32.
- Calibration and training folders exist, but autonomous navigation has not been fully validated in real tests.
- ESP32 firmware has multiple variants/snapshots for full robot, anchors, UWB calibration, and manual-control tests.

Not yet product-ready:

- UWB geometry must be redesigned around robot working area, not only playable court.
- Two-anchor UWB ambiguity must be handled with candidate tracking.
- Safety stops must be stricter before real autonomous tests.
- YOLO must be checked on the real court and likely trained on project-specific images.
- Logs must be collected during every physical test.

## Core Architecture

There are two rectangles:

- Playable court: standard tennis court with lines, net, service boxes, and gameplay zones.
- Robot working area: larger rectangle around the playable court where robot movement is allowed.

The playable court is inside the robot working area.

Balls may be outside playable court lines but still inside robot working area.

The robot must never drive outside the robot working area.

## UWB Architecture

The project currently uses only two UWB anchors.

Anchor placement:

- Anchor L and Anchor R are opposite diagonal corners of the robot working area.
- They are not necessarily corners of the playable court.
- They are not moved by the user in the first app implementation.
- The user physically places the anchors in real life according to the fixed app layout.

Calibration:

- User places the robot somewhere inside the real robot working area.
- User places the robot icon at the matching app position.
- User sets robot heading.
- App sends `robot_x`, `robot_y`, and `robot_heading` to ESP32.
- ESP32 selects the UWB candidate closest to the app pose and sets `calibration_valid=true`.

Runtime UWB tracking:

- Two anchors produce two candidate points.
- The accepted point is chosen by calibration and previous accepted pose.
- Impossible jumps are rejected.
- Repeated bad samples trigger STOP.
- Working-area boundary violations trigger STOP.

## Safety Rules

AUTO must not start if:

- Calibration is invalid.
- Heading is unknown.
- UWB is stale or lost.
- Current accepted pose is outside robot working area.

AUTO must stop if:

- Both anchors are lost.
- Ranges are stale.
- Position jumps are physically impossible.
- Candidate tracking becomes ambiguous or inconsistent.
- Accepted pose exits robot working area.
- Pi UART is stale while autonomous movement depends on Pi camera commands.

## Product Readiness

Current readiness:

- Manual control: usable prototype.
- Autonomous mode: not ready for unrestricted real-court use.
- Demo/grant readiness: possible only with constrained demo mode after safety and UWB branch tracking are implemented.
- Product readiness: prototype, not product.

## Development Priorities

1. Unify geometry: playable court, robot working area, anchors, app mapping.
2. Implement two-candidate UWB tracker with calibration branch lock.
3. Add strict safety stops.
4. Keep isolated firmware variants/modes for UWB calibration, manual motors, collector, camera-only collection, safety injection, and demo tests.
5. Run one-zone low-speed autonomous tests.
6. Validate camera and YOLO on real court images.
7. Build stable demo mode.
8. Improve collection and train custom YOLO after navigation is stable.
