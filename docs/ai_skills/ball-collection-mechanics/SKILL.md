---
name: ball-collection-mechanics
description: Use when working on collector relay timing, basket sensor, ball passage sensor, ball count, unloading, return-home behavior, or physical collection reliability.
---

# Ball Collection Mechanics

## Current Mechanism

ESP32 controls:

- Collector relay on pin 2.
- Basket sensor on pin 34.
- Ball passage sensor on pin 35.
- `ballsCollected` count.
- `maxBalls` capacity.
- `ST_UNLOADING` state.

## Current Behavior

Pi sends:

- `COLLECT` when the detected ball appears close enough by bounding-box size.

ESP32 then:

- Stops motors.
- Enables collector relay for a fixed time.
- Updates ball count via sensor interrupt.
- Returns to search or return/unload if full.

## Known Risks

- Bounding-box size is not reliable distance in real-world conditions.
- Collection success is not strongly confirmed by vision.
- Ball passage sensor debounce/noise needs physical validation.
- Collector relay timing may need tuning.

## Priority Rule

Do not optimize collection before navigation and safety are stable.

The first autonomous demo may stop near a ball without fully collecting it.

## Future Improvements

- Debounce ball sensor.
- Confirm collection by ball count and/or camera.
- Add collector current/relay diagnostics if hardware supports it.
- Tune relay timing from test logs.
- Add jam detection if the collector can stall.
