# 08 Demo Grant

## Demo Goal

Show a reliable, safe, repeatable prototype scenario. Do not try to prove full product autonomy too early.

## Recommended Demo Flow

1. Connect app to ESP32.
2. Show robot working area and diagonal anchors.
3. Show playable court inside working area.
4. Place robot and calibrate pose/heading.
5. Select one zone.
6. Start conservative demo mode.
7. Robot drives to zone.
8. Pi camera detects a tennis ball.
9. Robot approaches slowly or stops after detection.
10. App shows telemetry and final state.

## Investor/Grant Messaging

Emphasize:

- Working hardware integration.
- App control.
- UWB-based localization.
- Safety-first autonomous architecture.
- Expandable roadmap to 3/4 anchors and trained detection model.

Avoid overclaiming:

- Do not claim production autonomy until UWB reliability, safety stops, and real-court YOLO are validated.
- Do not claim two-anchor diagonal UWB is ambiguity-free.

## Demo Acceptance

Demo is acceptable when:

- It repeats 3 times without manual recovery.
- Any failure causes safe stop.
- App shows useful error/warning messages.
- Logs can explain what happened.
