---
name: geometry-and-calibration
description: Use when working on playable court geometry, robot working area geometry, app-to-real coordinate mapping, calibration pose, heading, or coordinate consistency across Flutter, ESP32, and calibration scripts.
---

# Geometry And Calibration

## Two Rectangles

The project uses two different rectangles:

- Playable court: standard tennis court.
- Robot working area: larger rectangle around the court.

Do not merge these concepts.

## Playable Court

Used for:

- Court rendering.
- Net position.
- Service boxes.
- Zone selection.
- Gameplay-related labels and targets.

Standard dimensions:

- Length: `23.77 m`
- Width: `10.97 m`
- Net: center along length.

## Robot Working Area

Used for:

- Robot localization.
- Movement boundary.
- Outside-court collection zones.
- UWB anchor placement.
- Safety stops.

The playable court is inside the robot working area. The working area may be larger than the playable court.

## Anchor Geometry

The project currently uses two UWB anchors:

- Anchor L is one diagonal corner of robot working area.
- Anchor R is the opposite diagonal corner of robot working area.
- They are not necessarily playable court corners.
- The app shows fixed anchor positions in the first implementation.
- The user physically places anchors according to the app layout.

## App Calibration

Calibration is not anchor placement.

Calibration means:

1. User places the real robot inside the working area.
2. User places the app robot icon at the matching app position.
3. User sets the robot heading.
4. User presses Calibrate.
5. App sends `robot_x`, `robot_y`, `robot_heading`.
6. ESP32 selects the nearest UWB candidate and marks calibration valid.

## Required Calibration Validation

ESP32 should reject calibration if:

- UWB candidates are unavailable.
- App pose is too far from both candidates.
- Heading is missing or invalid.
- App pose is outside robot working area.

Initial threshold:

- Calibration mismatch: `0.50-1.00 m`, tune from test logs.
