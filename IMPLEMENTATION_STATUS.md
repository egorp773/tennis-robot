# Implementation Status

Related notes: [[MEMORY]], [[DECISIONS]], [[AUTONOMY_ROADMAP]], [[TEST_LOG]], [[geometry-and-calibration]], [[uwb-two-anchor-diagonal]], [[esp32-safety]], [[flutter-app]], [[autonomous-mode]].

## Current Summary

Implementation has started. The project is moving from documentation alignment to code-level support for the two-anchor diagonal robot working area model.

Current state:

- Manual app control remains the strongest current capability.
- Autonomous mode now has stricter startup gates and safety stops, but it is not physically validated yet.
- ESP32, Flutter map/calibration, UWB anchor comments, and calibration scripts are being aligned around the robot working area model.
- UWB now needs real calibration data before any unrestricted autonomous test.
- Camera/YOLO still must be tested on the real court after navigation safety is verified.
- Local Flutter command execution recovered after deleting stale Flutter lock files and clearing proxy variables for the terminal session. `flutter pub get` works and `flutter analyze lib` is clean.

Core architecture to preserve:

- Playable court and robot working area are separate rectangles.
- Playable court is inside robot working area.
- Two UWB anchors are diagonal corners of robot working area.
- User does not drag anchors in the first app implementation.
- Calibration sends robot pose and heading, not anchor positions.
- Two-anchor UWB produces two candidates and requires branch tracking.

## Work Phases

| Phase | Status | Goal | Main Files |
|---|---|---|---|
| 0. Memory and planning | Done | Create long-term memory, skills, roadmap, and implementation tracker | `MEMORY.md`, `DECISIONS.md`, `AUTONOMY_ROADMAP.md`, `docs/ai_skills/*/SKILL.md`, this file |
| 1. Shared geometry | In progress | Define playable court, robot working area, anchor coordinates, and transforms consistently | ESP32 firmware, Flutter app, calibration scripts |
| 2. UWB two-candidate tracker | In progress | Replace hidden single-solution trilateration with candidate A/B, calibration branch lock, jump rejection | `tennis_robot.ino`, calibration scripts |
| 3. ESP32 safety stops | In progress | Block unsafe AUTO and stop on UWB/Pi/boundary/heading failures | `tennis_robot.ino`, Flutter telemetry |
| 4. Flutter map and calibration alignment | In progress | Show working area vs playable court correctly and send calibration in working-area meters | `court_zones_screen.dart`, `court_painter.dart`, `wifi_connection.dart` |
| 5. Calibration scripts update | In progress | Measure and analyze UWB in working-area coordinates | `calibration/measure.py`, `calibration/analyze.py`, `calibration/monitor.py` |
| 6. Test firmware variants | Pending | Keep isolated firmware variants/modes for UWB calibration, manual motors, collector, camera-only collection, safety injection, and demo tests | `esp32_firmware/*/` |
| 7. One-zone autonomous test support | Pending | Restrict first AUTO test to one small zone with conservative speed and boundaries | ESP32 + Flutter |
| 8. Vision validation | Pending | Test Pi camera and YOLO on real court | `main.py`, test logs |
| 9. Demo mode | Pending | Create short repeatable safe demo flow | ESP32 + Flutter + Pi |

## Done

- Created long-term project memory.
- Converted markdown memory into Codex Skill-style folders with `SKILL.md`.
- Preserved old flat AI skill notes in `docs/wiki/raw/old_ai_skills/`.
- Updated `AGENTS.md` to require reading memory, decisions, roadmap, and relevant skills.
- Added Obsidian-style links between memory files.
- Updated roadmap to the current two-anchor diagonal robot working area model.
- Updated main ESP32 firmware constants from net-post anchors to diagonal robot working area anchors.
- Added two-candidate UWB trilateration in the main ESP32 firmware.
- Added calibration branch selection using app-provided robot pose.
- Added runtime candidate selection using previous accepted pose.
- Added UWB jump rejection and repeated-reject STOP behavior.
- Added AUTO startup gates for calibration, heading, fresh UWB, accepted pose, zone count, and target boundary.
- Added STOP paths for UWB lost/stale, out-of-bounds pose, mirrored branch/jump rejection, and Pi timeout.
- Updated Flutter map model to render robot working area as the outer rectangle with playable court inside it.
- Updated Flutter calibration to send `CALIBRATE:heading:x:y` in robot working-area meters.
- Updated Flutter zone centers and delivery/home commands to use robot working-area coordinates.
- Removed the dev button that sent legacy empty `CALIBRATE`.
- Updated anchor firmware comments/coordinates for diagonal working-area anchors.
- Updated calibration measurement script and calibration README for the working-area geometry.
- Replaced stale architecture/Claude docs with the current robot working area and diagonal-anchor model.
- Documented the firmware-variant rule for isolated hardware tests.
- Added explicit firmware version/name/purpose output to existing ESP32 firmware variants.
- Added `esp32_firmware/collector_test/` for relay, basket sensor, ball passage sensor, and ball-count tests without motors/UWB.
- Added `esp32_firmware/camera_only_collection/` for Pi camera `TRACK/COLLECT/SEARCH` collection tests without UWB navigation.
- Made the main ESP32 firmware compile without the optional `MPU6050_light` library by falling back to app-provided calibration heading only.
- Fixed `.ino` compile issues caused by Arduino auto-generated prototypes around `Point2f`/`AutoSub`.
- Verified ESP32 compile for `tennis_robot`, `tennis_robot_cal`, `manual_control`, `collector_test`, `camera_only_collection`, `anchor_L`, and `anchor_R`.
- Verified `flutter pub get` completes after clearing stale environment/lock issues.
- Cleaned Flutter analyzer issues in `court_painter.dart` and `court_zones_screen.dart`; `flutter analyze lib` now reports no issues.
- Added non-driving ESP32 bench safety injection commands: `TEST_STOP:UWB_LOST`, `TEST_STOP:UWB_STALE`, `TEST_STOP:UWB_BRANCH`, `TEST_STOP:OUT_OF_BOUNDS`, and `TEST_STOP:PI_TIMEOUT`.
- Documented safety injection commands in `docs/ai_skills/esp32-safety/SKILL.md`.

## Current Verification Status

Verification is no longer blocked by a hung Flutter process, and Flutter static analysis is clean.

Recovered items:

- Deleted stale Flutter SDK lock files:
  - `C:\src\flutter\bin\cache\flutter.bat.lock`
  - `C:\src\flutter\bin\cache\lockfile`
- Ran Flutter with cleared proxy variables for the current terminal process.
- `flutter pub get` completed.
- `flutter analyze lib` completed with no issues.
- ESP32 firmware variants listed above compile.
- Full ESP32 firmware compiles after adding bench safety injection commands.

Remaining issues:

- No physical UWB, Pi, camera, or AUTO safety tests have been logged yet.

## Next Implementation Step

Start with Phase 3/4 verification cleanup, then continue toward Phase 6 safety/demo support.

Reason:

- Shared geometry, UWB branch tracking, safety gates, Flutter calibration, and calibration script alignment are now implemented enough to compile/check.
- Before real robot AUTO tests, the app analysis warnings should be cleaned or explicitly triaged, and ESP32 safety behavior should be verified with injected/staged failures.

Concrete next tasks:

1. Add one-zone conservative demo/test mode or confirm existing AUTO can be constrained from the app.
2. Run staged non-moving safety injection tests before any real movement.
3. Log all physical/integration test results in `TEST_LOG.md`.

## How Phase 1 Should Be Implemented

Implementation approach:

- Keep the first version simple and compile-time configured.
- Do not add draggable anchors yet.
- Do not add arbitrary user-editable working-area dimensions until the fixed model works.
- Use explicit names so `court_x/court_y` and `work_x/work_y` are not confused.
- Add helper functions or structs where possible instead of repeating coordinate math.

Expected code-level direction:

- ESP32: replace net-post anchor constants with robot working area anchor constants.
- ESP32: stop assuming both anchors share the same `X`.
- Flutter: render robot working area as outer area and playable court inside it.
- Flutter: send calibration pose in working-area meters.
- Calibration scripts: measure known points in working-area coordinates.

## How Phase 2 Should Be Implemented

UWB tracker behavior:

1. Given ranges to L/R, compute two candidates.
2. During calibration, choose candidate closest to app pose.
3. Store accepted pose and timestamp.
4. On each update, compute both candidates.
5. Reject candidates outside working area or requiring impossible movement.
6. Pick candidate closest to previous accepted pose.
7. Count rejected samples.
8. STOP after `3-5` consecutive rejected samples.

Important:

- Do not silently switch mirrored branch.
- Do not publish `POS` from an unvalidated candidate.
- Do not allow AUTO without accepted pose and heading.

## How Phase 3 Should Be Implemented

Safety behavior:

- AUTO_START must validate calibration, heading, UWB freshness, current pose, and target boundary.
- Any UWB stale/lost state must stop motors.
- Any out-of-bounds accepted pose must stop motors.
- Pi UART timeout during camera-guided movement must stop or return to safe search depending on mode.
- Every safety stop must send an app-visible reason.

Initial messages:

- `ERROR,NOT_CALIBRATED`
- `ERROR,HEADING_INVALID`
- `ERROR,UWB_LOST`
- `ERROR,UWB_STALE`
- `ERROR,UWB_JUMP`
- `ERROR,UWB_BRANCH`
- `ERROR,OUT_OF_BOUNDS`
- `WARN,PI_TIMEOUT`

## Verification Checklist

Before first real autonomous test:

- ESP32 compiles.
- Flutter analyzes/builds.
- Calibration command includes robot pose and heading.
- AUTO_START is rejected without calibration.
- Simulated stale UWB stops motors.
- Simulated impossible UWB jump is rejected or stops motors.
- Simulated out-of-bounds pose stops motors.
- App displays safety errors.
- One-zone AUTO can be selected without full-court behavior.

Firmware variant checklist:

- Full firmware: `esp32_firmware/tennis_robot/`.
- UWB calibration firmware/mode: raw ranges and candidate positions, minimal or disabled motor output.
- Manual motor test firmware/mode: motor commands without autonomous navigation.
- Collector test firmware/mode: `esp32_firmware/collector_test/`.
- Camera-only collection firmware/mode: `esp32_firmware/camera_only_collection/`.
- Safety test firmware/mode: inject stale UWB, impossible jumps, out-of-bounds, Pi timeout, and verify STOP reasons.
- Demo firmware/mode: one-zone, low speed, strict timeouts, readable app errors.

## Physical Test Gate

Do not run unrestricted AUTO until:

- UWB calibration has been measured and logged in `TEST_LOG.md`.
- Robot can stop safely on UWB failure.
- Robot can stop safely on Pi failure.
- One-zone route is constrained.
- Physical emergency cutoff is available.

## Open Questions

- What initial robot working area size should be used around the playable court?
- Where exactly inside the working area should the playable court be placed?
- Should home/unloading point be in working-area coordinates or court-relative coordinates?
- What maximum real robot speed should be used for jump rejection?
- What stopping distance should define the boundary slow/stop margin?

## Status Legend

- Done: implemented or documented and no longer active.
- Next: immediate work item.
- Pending: planned but not started.
- Blocked: needs user decision or physical test data.
