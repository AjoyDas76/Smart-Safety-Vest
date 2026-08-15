# Phase-04 GPS

## Objective
Integrate the NEO-M8N GPS module with the ESP32 to track the worker's real-time location, speed, and movement.

## Tasks

- [x] 4.1 GPS Module Integration — `4.1_GPS_Module_Integration.ino`
- [x] 4.2 Latitude & Longitude Reading — `4.2_Latitude_Longitude_Reading.ino`
- [ ] 4.3 GPS Accuracy Validation
- [x] 4.4 GPS Status Monitoring — `Phase_4.4_GPS_Status_Monitoring.ino`
- [x] 4.5 Distance Calculation — `Phase_4.5_Distance_Calculation.ino`
- [x] 4.6 Worker Speed Detection — `Phase_4.6_Worker_Speed_Detection.ino`
- [ ] 4.7 Worker Movement Detection
- [ ] 4.8 GPS Data Optimization
- [ ] 4.9 GPS Module Finalization

## Components
- ESP32 DevKit V1
- NEO-M8N GPS Module

## Wiring

| GPS (NEO-M8N) | ESP32 |
|----------------|--------|
| VCC | 3.3V / 5V |
| GND | GND |
| TX | GPIO16 (RX2) |
| RX | GPIO17 (TX2) |

## Code
See files listed under Tasks above.

## Test Results
_(Pending — to be filled in once 4.3 Accuracy Validation is done.)_

## Notes
- 4.3 (Accuracy Validation) should compare GPS-reported position against a known fixed point (e.g. HDOP + repeated static readings) to quantify real-world accuracy.
- 4.7 (Worker Movement Detection) is a step beyond 4.6's speed thresholds — likely combining GPS speed with MPU6050 motion state (Phase 3) to distinguish "walking" vs "just GPS drift while standing still."
- 4.9 (Finalization) should merge 4.1–4.8 into one clean, reusable GPS module (similar to how Phase 3.8 finalized MPU6050) ready to be dropped into Phase 5/9 integration.

## Status

⏳ **In Progress (5/9 sub-tasks complete)**
