# Phase 03 - MPU6050 Motion &  & Fall Detection

## Phase Information

| Item | Details |
|------|---------|
| Project | Smart Safety Vest |
| Phase | Phase 03 |
| Module | MPU6050 Motion Detection |
| Version | v1.1 |
| Status | ✅ Completed |
| Author | Ajoy Das Team |

---

## Objective

The objective of this phase is to establish communication between the ESP32 and the MPU6050 sensor and acquire real-time motion data. This phase covers reading the MPU6050 IMU, filtering the data, calculating body tilt, detecting worker posture (standing / leaning / lying), detecting falls, and classifying movement (standing / walking / running).

---

## Hardware Used

- ESP32 DevKit V1
- MPU6050 (GY-521)
- Jumper Wires
- USB Cable

---

## Pin Connection

| MPU6050 | ESP32 |
|----------|--------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

> Note: XDA, XCL, AD0, and INT pins are not connected.

---

## Software Requirements

- Arduino IDE 2.x
- ESP32 Board Package
- Wire Library
- MPU6050 Library

---

## Features

- ESP32 and MPU6050 communication
- I2C interface initialization
- Raw accelerometer data acquisition
- Raw gyroscope data acquisition
- Real-time Serial Monitor output
- Stable sensor initialization

---

## Library Dependencies
 
- `Wire.h` (built-in)
- `MPU6050.h` (I2Cdevlib / Electronic Cats MPU6050 library)
- `math.h` (built-in)
Install the MPU6050 library via Arduino Library Manager or from [I2Cdevlib](https://github.com/jrowberg/i2cdevlib).
 
## Files
 
| File | Description |
|---|---|
| `3.1_MPU6050_Test.ino` | Verifies I2C communication with the MPU6050 and streams raw accelerometer (`ax, ay, az`) and gyroscope (`gx, gy, gz`) values to the Serial Monitor. |
| `3.2_Reference_Calibration.ino` | Captures the worker's normal standing position at startup (500-sample average) and stores it as a reference (`reference.ax/ay/az/gx/gy/gz`) for later comparison (`dAX, dAY, dAZ`). |
| `3.3_Moving_Average_Filter.ino` | Applies a 10-sample moving average filter to accelerometer data (`axBuffer`, `ayBuffer`, `azBuffer`) to smooth out sensor noise, printing raw vs. filtered values. |
| `3.4_Angle_Calculation.ino` | Combines the moving average filter with `atan2()` based math to calculate **Pitch** and **Roll** angles (in degrees) from filtered accelerometer data. |
| `3.5_Auto_Calibration.ino` | Automates the calibration step — takes 100 samples after a 5-second countdown and computes `refPitch` / `refRoll` automatically, then continuously reports `tiltPitch` / `tiltRoll` relative to that reference. |
| `3.6_Motion_State.ino` | Introduces a `WorkerState` state machine (`STANDING`, `FORWARD_LEAN`, `BACKWARD_LEAN`, `LEFT_LEAN`, `RIGHT_LEAN`) driven by pitch/roll thresholds (±20°), with state-change detection so output only prints on transitions. |
| `3.7_Fall_Detection_Engine.ino` | Full fall-detection pipeline: **Free Fall** detection (acceleration below `0.80 g`), **Impact** detection (acceleration above `1.8 g`), **Lying Position** detection (tilt > 70° held for 1.5s), and a **Fall Latching** system that requires free-fall + impact + lying within a 6-second window to confirm `FALL_DETECTED`, with auto-recovery once the worker stands back up. |
| `3.8 Worker_Movement_Detection.ino` | Fuses posture detection (from 3.6/3.7) with a gyroscope motion-energy classifier for **STANDING / WALKING / RUNNING**. Movement classification is *gated* by posture — it only runs while the worker is upright, so bending/leaning is never misread as walking or running. Reporting priority: `FALL > LYING > LEAN > WALKING/RUNNING/STANDING`. |
| `3.9_MPU6050_Module_Finalization.ino` | Final, consolidated module. Wraps all of the above into a single `MPUData` struct and clean functions (`initMPU()`, `calibrateMPU()`, `updateMPUFilter()`, `updatePostureState()`, `updateMovementState()`, `updateFallDetection()`, `updateMPUData()`, `printMPUData()`) — a ready-to-integrate interface for the rest of the firmware. |
 
## Key Thresholds
 
| Parameter | Value |
|---|---|
| Pitch / Roll lean threshold | 15° / 20° |
| Free fall threshold | < 0.80 g |
| Impact threshold | > 1.8 g |
| Lying angle | > 70° (held 1.5s) |
| Fall confirmation window | 6000 ms |
| Moving average filter size | 10 samples |
| Movement energy window | 15 samples |
| Standing energy max | 1500 |
| Walking energy max | 6000 (above = running) |
 
## Final Output (3.9)
 
```
--------------------------------
Worker Status : STANDING
Upright (gate): YES
Motion Energy (avg) : 320
TiltPitch: 1.20 TiltRoll: -0.85
FreeFall: NO Impact: NO Lying: NO
Fall: NO
Acceleration G: 1.01
```
 
## Usage
 
1. Open the desired `.ino` file in Arduino IDE.
2. Select the ESP32 board and correct COM port.
3. Upload and open Serial Monitor at **115200 baud**.
4. For calibration steps (3.2, 3.5, 3.6+), keep the worker/vest still during the countdown.
```
