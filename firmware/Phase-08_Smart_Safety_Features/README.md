# Phase-08 Smart Safety Features

## Objective
Add the on-body safety hardware features to the worker unit: manual SOS button, audible/visual alarms, and power management (solar charging + battery monitoring), and finalize the safety module as a whole.

## Tasks

- [ ] 8.1 SOS Button
- [ ] 8.2 Buzzer Alarm
- [ ] 8.3 LED Warning Indicator
- [ ] 8.4 Battery Monitoring
- [ ] 8.5 Power Management
- [ ] 8.6 Low Battery Alert
- [ ] 8.7 Safety Module Finalization

## Components
- SOS Push Button
- Active Buzzer
- Status LED(s)
- 18650 Li-ion Battery
- 5V Flexible Solar Panel
- DFRobot Solar Power Manager 5V

## Wiring
_(To be documented — SOS button and buzzer wiring already exist informally in the Phase 5 LoRa transmitter sketches (`BUTTON_PIN 4`); this phase should formalize and expand that into a standalone safety module, and add the solar/battery monitoring wiring.)_

## Code
_(To be added. Note: 8.1 SOS Button logic already exists inside the Phase-05 LoRa transmitter sketches (e.g. `5.4_Transmitter_WorkerStatus.ino`, `5_8_Transmitter_Reliability.ino`) — reuse/refactor from there rather than starting from scratch.)_

## Test Results
_(Pending)_

## Notes
- This phase merges what were previously two separate, empty placeholder folders (`Phase-06_Power_System` and `Phase-07_SOS`) into one, matching the updated roadmap.
- Buzzer Alarm (8.2) and LED Warning (8.3) should trigger on both **SOS** and **FALL_DETECTED** states from Phase 3/5.

## Status

⏳ **Pending**
