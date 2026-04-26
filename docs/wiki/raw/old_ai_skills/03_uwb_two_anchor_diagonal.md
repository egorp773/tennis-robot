# 03 UWB Two-Anchor Diagonal Model

## Anchor Placement

The current prototype uses two UWB anchors:

- Anchor L: one diagonal corner of robot working area.
- Anchor R: opposite diagonal corner of robot working area.

They are not necessarily on playable court corners.

The user physically places anchors according to the app layout.

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

## Future Upgrade

Three or four anchors would reduce ambiguity and improve product reliability, but current work should support the two-diagonal-anchor model first.
