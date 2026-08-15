# Phase-06 IoT Cloud Platform

## Objective
Connect the Smart Safety Vest to the internet and stream live worker data (sensor readings, GPS location, fall/SOS alerts) to a cloud dashboard so the base station / supervisor can monitor workers remotely, not just over LoRa range.

## Tasks

- [ ] 6.1 WiFi Connection
- [ ] 6.2 Firebase Integration
- [ ] 6.3 Live Sensor Upload
- [ ] 6.4 Live GPS Upload
- [ ] 6.5 Fall Alert Upload
- [ ] 6.6 Data Storage & Dashboard
- [ ] 6.7 Cloud Security
- [ ] 6.8 IoT Module Finalization

## Components
- ESP32 DevKit V1 (WiFi-enabled base station or gateway unit)
- Firebase Realtime Database / Firestore project

## Wiring
_(No new hardware wiring — this phase is software/cloud integration on top of the existing base station ESP32.)_

## Code
_(To be added — target: `WiFi_Firebase_Gateway.ino` or similar, receiving data from the LoRa receiver and forwarding it to Firebase.)_

## Test Results
_(Pending)_

## Notes
- This phase runs on the **base station** (receiver) side, taking the data already decoded in Phase 5 (LoRa) and pushing it to the cloud.
- Cloud Security (6.7) should cover: Firebase rules/auth, not hardcoding WiFi/Firebase credentials in committed code (use a `secrets.h` excluded via `.gitignore`).

## Status

⏳ **Pending**
