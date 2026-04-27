---
name: autonomous-mode
description: Use when working on AUTO behavior, zone navigation, sweep/search, Pi-guided tracking, collection state transitions, returning home, or first constrained autonomous tests.
---

# Autonomous Mode

## Current AUTO Concept

ESP32 AUTO mode should eventually:

1. Navigate to selected zone.
2. Sweep/search inside the zone.
3. Use Pi `TRACK` commands to approach visible ball.
4. Trigger collector on `COLLECT`.
5. Return home or unloading point when full or commanded.

## First Safe AUTO Scope

Do not start with full-court autonomy.

First test scope:

- One small zone.
- Low speed.
- Tight working-area boundary.
- Physical emergency cutoff.
- Collector optional or disabled.
- Clear STOP condition.

## Required Preconditions

AUTO requires:

- Valid calibration.
- Valid heading.
- Valid UWB accepted pose.
- Safe target inside robot working area.
- Pi UART healthy if tracking/search depends on Pi.

## Demo Mode

Demo mode should be more conservative than normal AUTO:

- One zone.
- Short route.
- Low speed.
- Timeout on every stage.
- Stop near ball or after detection, not necessarily full collection.

Current implementation:

- `DEMO_START` is separate from `AUTO_START`.
- It is valid only with one selected zone.
- It forces low navigation speed.
- It stops after one zone sweep/collection or on demo timeout.

## Deferred

Defer these until navigation is stable:

- Full-court cleaning.
- Multi-zone optimization.
- Real multi-ball route planning.
- Aggressive collection behavior.
- Custom model tuning beyond basic validation.
