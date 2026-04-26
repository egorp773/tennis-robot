# Tennis Robot Autonomy Roadmap

Related notes: [[MEMORY]], [[DECISIONS]], [[TEST_LOG]], [[geometry-and-calibration]], [[uwb-two-anchor-diagonal]], [[testing-and-logs]].

## Goal

Bring the project from manual-control prototype to a safe, testable autonomous demo.

Primary principle: navigation and safety must be validated before improving ball collection quality.

## Coordinate System

There are two separate rectangles:

- Playable court: standard tennis court, used for net, service boxes, court zones, and game logic.
- Robot working area: larger rectangle around the court, used as the robot movement boundary and outside-ball collection area.

Canonical playable court coordinates:

- `court_x`: court length, meters, `0.00 .. 23.77`
- `court_y`: court width, meters, `0.00 .. 10.97`
- Net: `court_x = 11.885`
- Court center: `(11.885, 5.485)`

Canonical robot working area coordinates:

- `work_x`: working-area length axis, meters.
- `work_y`: working-area width axis, meters.
- The playable court is placed inside this working rectangle.
- App mapping must be explicit: screen vertical axis maps to one real axis, screen horizontal axis maps to the other real axis.
- Flutter, ESP32, calibration scripts, and docs must use the same orientation and units.

Target anchor layout for 2-anchor prototype:

- Use two UWB anchors as diagonal corners of the robot working area, not necessarily the playable court.
- The playable tennis court is inside the larger robot working rectangle.
- Anchor L and Anchor R are shown in the app as opposite diagonal corners of the working area.
- The user physically places the anchors in real life according to the app layout.
- The app does not need to let the user drag anchors manually in the first implementation.
- The robot working area may be larger than the playable court to allow collection of balls outside court lines.
- Diagonal two-anchor UWB has a two-candidate ambiguity, so it requires calibration, branch locking, jump rejection, boundary checks, and safety stops.

Reason:

- Two anchors give two possible circle intersections.
- Calibration selects the correct initial candidate by comparing UWB candidates with the robot pose selected in the app.
- During motion, the tracker must keep the candidate closest to the last accepted position.
- A physically impossible jump must be rejected or cause STOP.
- The allowed robot working rectangle is a virtual boundary. Anchors help define/localize it, but the stop is enforced in software.
- This diagonal-anchor workflow is acceptable for prototype/demo only if safety logic is strict.
- For future product reliability, 3 or 4 anchors may still be better.

## Two-Anchor Diagonal Working Area Model

Definitions:

- Playable court: standard tennis court with lines, net, service boxes, and court zones.
- Robot working area: larger rectangle around the court where robot may collect balls.
- Anchor L/R: opposite diagonal corners of the robot working area.
- Robot pose calibration: user places robot in the same real/app position and sets heading.
- Allowed movement boundary: the robot working area, possibly with margins or a safety shrink.

Required logic:

- Do not treat playable court dimensions as the same as working area dimensions.
- Court zones are inside the playable court.
- Outside collection zones may be inside the working area but outside playable court lines.
- AUTO targets must be checked against the working area boundary.
- UWB candidates must be resolved using calibration and previous accepted pose.
- Do not start AUTO if calibration is invalid.
- Heading must be included in calibration and must be valid before AUTO.

Two-candidate UWB tracker:

1. Trilateration from two anchors returns `candidate_A` and `candidate_B`.
2. During calibration, choose the candidate closer to the app-provided `robot_x,robot_y`.
3. Store the accepted pose and selected branch/side after calibration.
4. For every new range pair, compute both candidates again.
5. Choose the candidate closer to the previous accepted pose.
6. Reject candidates that require a physically impossible jump.
7. Prefer candidates inside the working area.
8. Optionally compare candidate motion with current heading and speed once motor feedback is trustworthy.
9. If several consecutive samples are rejected, STOP with a clear safety reason.
10. If accepted pose exits the working area, STOP.

Calibration payload from app to ESP32 should include at minimum:

- `robot_x`
- `robot_y`
- `robot_heading`
- `calibration_valid=true`
- Working-area parameters if they are not compile-time constants.

## Phase 1 - Fix Shared Geometry

Files likely involved:

- `esp32_firmware/tennis_robot/tennis_robot.ino`
- `esp32_firmware/anchor_L/anchor_L.ino`
- `esp32_firmware/anchor_R/anchor_R.ino`
- `tennis_app/lib/painters/court_painter.dart`
- `tennis_app/lib/screens/court_zones_screen.dart`
- `calibration/measure.py`
- `calibration/analyze.py`
- `AGENTS.md`
- `ARCHITECTURE.md`

Tasks:

- Define playable court geometry.
- Define robot working area geometry.
- Define fixed app positions of diagonal anchors in the working area.
- Ensure Flutter, ESP32, calibration scripts, and docs agree on the playable court rectangle.
- Ensure Flutter, ESP32, calibration scripts, and docs agree on the working area rectangle.
- Ensure Flutter, ESP32, calibration scripts, and docs agree on anchor L/R coordinates.
- Ensure Flutter, ESP32, calibration scripts, and docs agree on app screen mapping to real meters.
- Ensure Flutter, ESP32, calibration scripts, and docs agree on robot pose calibration format.
- Ensure boundary checks use the working area, not the playable court.
- Replace trilateration logic that assumes equal anchor `X` or net-post placement.

Done when:

- App, ESP32, calibration scripts, and docs all use the same playable court and working area coordinates.
- A known robot point on the app map corresponds to the same ESP32 working-area coordinate.
- Anchor markers in the app match physical diagonal working-area anchor placement.
- Playable court zones are rendered inside the working area with correct offsets.
- AUTO targets outside the working area are rejected.

## Phase 2 - UWB Calibration

Tasks:

- Reflash robot and anchors with agreed working-area geometry.
- Collect measurements at 8-12 known points.
- Include working-area corners, playable court corners, center, service boxes, baselines, and selected outside zones.
- During calibration, compare app-provided robot pose to both UWB candidates.
- Reject calibration if nearest candidate is too far from app-provided pose.
- Run `calibration/analyze.py`.
- Record RMS and systematic errors.

Acceptance target:

- Initial calibration mismatch threshold: start with `0.50-1.00 m`.
- Minimum for cautious autonomous test: `RMS 2D < 0.50 m`
- Target for reliable demo: `RMS 2D < 0.30 m`
- If `RMS 2D >= 1.00 m`, do not run real autonomous navigation except constrained safety tests.

## Phase 3 - Safety Stops

ESP32 safety requirements:

- Stop or reject AUTO if calibration is invalid.
- Stop or reject AUTO if heading is unknown.
- Stop if both UWB anchors are lost.
- Stop if one or both ranges become stale beyond timeout.
- Stop if position jumps more than a physically possible threshold.
- Stop or reject if UWB candidate switches to the mirrored branch with an impossible jump.
- Stop if both UWB candidates are inconsistent with last accepted pose.
- Stop if accepted pose is outside the robot working area.
- Slow or stop if robot approaches the working area boundary too closely.
- Stop if Pi UART messages are stale while autonomous search/tracking is active.
- Stop if app calibration pose and UWB candidate mismatch more than the allowed threshold during calibration.
- Send clear WebSocket error/warning messages to the app.

Suggested initial thresholds:

- UWB stale timeout: `2-5 s`
- Calibration mismatch threshold: `0.50-1.00 m`
- Position jump: tune after logs, start conservative around `1.0-2.0 m` per update window
- Consecutive rejected UWB samples before STOP: `3-5`
- Boundary margin: add slow/stop margin near the working area edge
- Pi timeout in autonomous movement: `3-5 s` for first tests, not `15 s`
- First-test boundary: one selected zone plus a small margin

Done when:

- Disconnecting an anchor stops the robot.
- Stopping Pi process stops the robot.
- Injecting an impossible coordinate jump rejects the sample or stops the robot.
- Mirrored-branch jumps are rejected.
- Leaving the working area stops the robot.
- AUTO cannot start without valid robot pose and heading calibration.
- App shows a useful error state.

Implementation items to verify:

- Trilateration returns two candidates, not one hidden solution.
- Calibration selects the initial branch.
- Runtime tracker chooses the candidate nearest to previous accepted pose.
- Jump rejection exists.
- Boundary check uses working area, not playable court.
- App and ESP32 agree on coordinate orientation.
- Heading is included in calibration.
- Clear safety reason is sent to app/log.

## Phase 4 - First Autonomous Zone Test

## Phase 4A - Test Firmware Variants

Purpose:

Keep isolated firmware variants or compile-time test modes for hardware tests. Do not force every subsystem test through the full autonomous firmware.

Current variants:

- `esp32_firmware/tennis_robot/`: full integrated robot firmware.
- `esp32_firmware/anchor_L/`: UWB anchor L.
- `esp32_firmware/anchor_R/`: UWB anchor R.
- `esp32_firmware/tennis_robot_cal/`: UWB calibration/range-focused firmware.
- `esp32_firmware/manual_control/`: isolated manual-control firmware.
- `esp32_firmware/collector_test/`: collector relay/sensor/counter test firmware.
- `esp32_firmware/camera_only_collection/`: Pi camera guided collection without UWB.
- `esp32_firmware/1000__working/`: preserved known-working snapshot.

Required/planned variants or modes:

- UWB calibration: raw ranges, two UWB candidates, accepted pose, no autonomous movement.
- Manual motor test: joystick/serial motor commands, no AUTO.
- Collector test: implemented in `collector_test`.
- Camera-only collection: implemented in `camera_only_collection`.
- Safety injection: stale UWB, impossible jump, out-of-bounds pose, Pi timeout, STOP reason checks.
- Demo mode: one zone, low speed, short timeout, strict boundary.

Every test firmware/mode must document:

- purpose;
- moving hardware enabled;
- safety limits;
- expected commands;
- what to record in `TEST_LOG.md`.

Done when:

- Existing firmware variants are named and documented.
- Missing test variants are either implemented or explicitly marked as planned.
- Physical tests can be run one subsystem at a time.

Constraints:

- One small selected zone only.
- Low speed mode.
- No people near the robot.
- Physical emergency power cutoff available.
- Collector can be disabled for first run.

Scenario:

1. Place anchors at opposite diagonal corners of the robot working area as shown in the app.
2. Calibrate robot pose from app.
3. Send one zone.
4. Start AUTO.
5. Robot drives to zone center.
6. Robot performs sweep/search.
7. If ball appears, Pi sends `TRACK`.
8. Robot approaches slowly.
9. Robot stops on `COLLECT` or demo stop condition.

Done when:

- Robot reaches the zone without leaving boundary.
- Robot does not continue moving after UWB/Pi failure.
- Logs explain every state transition.

## Phase 5 - Camera And YOLO Validation

Tasks:

- Test `main.py --test` on Pi.
- Test `main.py --no-display` with MJPEG stream.
- Capture real court frames with balls at multiple distances.
- Test day, shadow, line, net, and background cases.
- Compare current `yolov8n.pt` against real images.
- Decide whether custom `best.pt` is required.

Expected issue:

- COCO `sports ball` is not reliable enough for tennis balls on real courts.
- Custom model will probably be needed after navigation is stable.

Done when:

- Detection works on real court frames at the distances needed for tracking.
- False positives from lines/net/background are understood.
- There is a decision: keep COCO temporarily or train custom model.

## Phase 6 - Demo Mode

Goal:

Create a short, stable scenario for investor/grant demo.

Demo flow:

1. Connect app to ESP32.
2. Show anchors online.
3. Calibrate robot on map.
4. Select one zone.
5. Robot drives to zone.
6. Camera detects ball.
7. Robot approaches or stops near the ball.
8. App shows state, position, battery, ball count/log.

Demo mode should be more conservative than full AUTO:

- One zone.
- Low speed.
- Tight boundary.
- Short timeout.
- Clear stop condition.

Done when:

- Demo can be repeated 3 times in a row without manual recovery.
- Failure produces safe stop and readable error.

## Phase 7 - Logging

Log fields to capture:

- Timestamp
- ESP32 state and sub-state
- UWB range L/R
- Computed UWB position
- Anchor online status
- Pi command received
- Motor command sent
- App command received
- YOLO detection count/confidence/bbox
- Safety stop reason

Preferred approach:

- ESP32 serial log for state/UWB/motor/safety.
- Pi log for camera/detection/UART.
- App terminal log for WebSocket TX/RX.
- Later: unified CSV/JSON test log.

Done when:

- After a failed run, the cause can be classified as UWB, camera, Pi link, motor control, app command, or algorithm.

## Deferred Until Navigation Is Stable

- Improve collector mechanics and timing.
- Train and deploy custom YOLO model.
- Multi-ball route planning in world coordinates.
- Full-court autonomous cleaning.
- Advanced obstacle detection.
- Product UI cleanup.
- Multi-anchor 3/4-anchor UWB upgrade.

## Current Product Readiness Estimate

- Manual control: usable prototype.
- App telemetry: partial prototype.
- UWB localization: needs redesign and recalibration.
- Autonomous mode: not ready for unrestricted real-court use.
- Product readiness: prototype, not product.
- Investor demo readiness: possible only with constrained demo mode.
