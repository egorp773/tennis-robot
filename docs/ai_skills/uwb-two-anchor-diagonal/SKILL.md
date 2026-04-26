---
name: uwb-two-anchor-diagonal
description: Use when working on UWB localization, two-anchor diagonal robot working area, calibration candidate selection, branch tracking, jump rejection, and UWB safety stops.
---

# UWB Two-Anchor Diagonal Model

## Anchor Placement

The current prototype uses two UWB anchors:

- Anchor L: one diagonal corner of robot working area.
- Anchor R: opposite diagonal corner of robot working area.

They are not necessarily on playable court corners.

The user physically places anchors according to the app layout. The app does not need draggable anchors in the first implementation.

## Mathematical Ambiguity

Two anchors give two circle intersections:

- `candidate_A`
- `candidate_B`

The diagonal layout does not remove this ambiguity. It is acceptable for prototype/demo only with strict tracking and safety.

## Required Tracker

Trilateration must return two candidates.

At calibration:

- Compare both candidates with app-provided `robot_x,robot_y`.
- Select the nearest candidate.
- Store it as the first accepted pose.
- Store branch/side if useful.
- Set `calibration_valid=true` only after pose and heading pass validation.

At runtime:

- Compute both candidates for every fresh range pair.
- Prefer the candidate closest to the previous accepted pose.
- Reject candidates outside working area unless recovering in a controlled state.
- Reject jumps that exceed physical limits.
- Increment rejected-sample count on bad data.
- STOP after several consecutive rejects.

## Safety Conditions

STOP if:

- Both anchors are lost.
- Ranges are stale.
- Both candidates are inconsistent with previous pose.
- Candidate branch switches via impossible jump.
- Accepted pose exits working area.
- Rejected UWB samples exceed threshold.

## Required AUTO Gate

AUTO is forbidden without:

- `calibration_valid=true`
- Valid `robot_heading`
- Fresh accepted UWB pose
- Target inside robot working area

## Future Upgrade

Three or four anchors would reduce ambiguity and improve product reliability, but current work should support the two-diagonal-anchor model first.
