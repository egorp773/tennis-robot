# DECISIONS.md

This file records project decisions that should not be rediscovered every session.

Related notes: [[MEMORY]], [[AUTONOMY_ROADMAP]], [[uwb-two-anchor-diagonal]], [[geometry-and-calibration]].

## Decision 001 - Separate Playable Court And Robot Working Area

Status: accepted.

The playable tennis court and robot working area are different rectangles.

- Playable court is the standard tennis court used for lines, net, service boxes, and game zones.
- Robot working area is a larger rectangle around the court where the robot is allowed to move and collect balls.
- Outside collection zones may be outside playable court lines but must be inside robot working area.

Reason:

Tennis balls can leave the court. The robot needs a larger software boundary than the official court lines.

## Decision 002 - Current Anchor Model Is Two Diagonal Working-Area Anchors

Status: accepted.

The current two-anchor plan is:

- Anchor L and Anchor R are opposite diagonal corners of robot working area.
- They are not necessarily on playable court corners.
- They are not necessarily on the same baseline.
- The app shows the fixed anchor layout.
- The user physically places the anchors according to the app layout.

This replaces the earlier idea that the primary plan should move anchors to neighboring baseline corners.

## Decision 003 - Do Not Make Draggable Anchors In First Version

Status: accepted.

The first implementation does not require user-draggable anchors.

Reason:

The physical setup should match the fixed app layout. Reducing configurability reduces UI complexity and prevents inconsistent app/ESP32 geometry.

## Decision 004 - Two-Anchor Diagonal UWB Requires Branch Tracking

Status: accepted.

Two anchors produce two possible candidate positions. The diagonal layout is acceptable for prototype/demo only with:

- Calibration against app-provided robot pose.
- Initial branch selection.
- Runtime candidate selection by previous accepted pose.
- Jump rejection.
- Consecutive-bad-sample STOP.
- Working-area boundary STOP.
- AUTO forbidden without valid calibration and heading.

The diagonal model must not be described as mathematically unambiguous.

## Decision 005 - Future Product May Need 3 Or 4 Anchors

Status: accepted.

Three or four anchors remain a likely future improvement for product reliability.

Reason:

More anchors reduce ambiguity and improve robustness. However, current work should support the two-diagonal-anchor workflow because that is the target prototype architecture.

## Decision 006 - Navigation Before Collection Optimization

Status: accepted.

Do not prioritize ball collection mechanics or custom YOLO training before navigation safety is validated.

Order:

1. Geometry.
2. UWB tracking.
3. Safety stops.
4. One-zone autonomous test.
5. Camera validation.
6. Demo mode.
7. Collection and model quality improvements.

## Decision 007 - Keep Separate Firmware Variants For Hardware Tests

Status: accepted.

The project should keep separate firmware variants or explicit compile-time test modes for isolated hardware tests.

Current examples:

- Full robot firmware: `esp32_firmware/tennis_robot/`.
- UWB calibration/range firmware: `esp32_firmware/tennis_robot_cal/`.
- Manual-control firmware: `esp32_firmware/manual_control/`.
- Collector-only firmware: `esp32_firmware/collector_test/`.
- Camera-only collection firmware: `esp32_firmware/camera_only_collection/`.
- UWB anchor firmware: `esp32_firmware/anchor_L/` and `esp32_firmware/anchor_R/`.
- Preserved known-working snapshot: `esp32_firmware/1000__working/`.

Required/planned test variants or modes:

- UWB calibration without autonomous movement.
- Manual motor control without autonomous navigation.
- Collector-only relay/sensor/ball-count test.
- Camera-only collection using Pi `TRACK/COLLECT/SEARCH` without UWB navigation.
- Safety injection for stale UWB, impossible jumps, out-of-bounds, Pi timeout, and STOP reason checks.
- Conservative one-zone demo mode.

Reason:

Hardware debugging must be isolated. A camera test, collector test, motor test, UWB calibration, or safety-injection test should not require the full autonomous stack unless the goal is full integration testing.
