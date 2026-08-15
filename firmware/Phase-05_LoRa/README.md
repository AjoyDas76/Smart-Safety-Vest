# Phase-05 LoRa Communication System (SX1278 RA-02)

## Objective
Establish long-range wireless communication between the worker's vest (transmitter) and a base station (receiver) using the SX1278 RA-02 LoRa module, carrying worker status, GPS location, and fall/SOS alerts reliably.

## Tasks

- [x] 5.1 SX1278 RA-02 Integration — `5.1_Integration_Test2.ino`
- [x] 5.2 LoRa Transmitter Setup (Safety Vest) — `5.2_Transmitter_Setup2.ino`
- [x] 5.3 LoRa Receiver Setup (Base Station) — `5.3_Receiver_Setup2.ino`
- [x] 5.4 Worker Status Transmission — `5.4_Transmitter_WorkerStatus.ino` / `5.4_Receiver_WorkerStatus.ino`
- [x] 5.5 GPS Data Transmission — `5.5_Transmitter_GPS.ino` / `5.5_Receiver_GPS.ino`
- [x] 5.6 Fall Alert Transmission — `5_6_Transmitter_FallAlert.ino` / `5_6_Receiver_FallAlert.ino`
- [ ] 5.7 Communication Range Testing — ⚠️ only receiver side done (`5_7_Receiver_RangeTest.ino`); transmitter-side range test sketch missing
- [x] 5.8 Packet Reliability & Error Handling — `5_8_Transmitter_Reliability.ino` / `5_8_Receiver_Reliability.ino`
- [ ] 5.9 LoRa Communication Module Finalization

## Components
- ESP32 DevKit V1 (x2 — one worker unit, one base station)
- SX1278 RA-02 LoRa Module (x2)
- SOS Push Button
- NEO-M8N GPS Module (worker unit)

## Wiring

| SX1278 RA-02 | ESP32 |
|---------------|--------|
| VCC | 3.3V |
| GND | GND |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| NSS | GPIO5 |
| RESET | GPIO14 |
| DIO0 | GPIO26 |

SOS Button: one leg → GPIO4, other leg → GND (internal pull-up).

## Code
See files listed under Tasks above.

## Test Results
_(Pending full range-test summary — see 5.7.)_

## Notes
- 5.7 needs a matching **transmitter-side** range-test sketch (currently only the receiver logs RSSI/SNR/packet-loss). The transmitter just needs to send numbered packets at a fixed interval for the receiver to evaluate.
- 5.9 (Finalization) should merge 5.6 (Fall Alert) + 5.8 (Reliability) into one final transmitter/receiver pair — this final pair is effectively what Phase 9 (System Integration) will build on.

## Status

⏳ **In Progress (7/9 sub-tasks complete, 1 partial)**
