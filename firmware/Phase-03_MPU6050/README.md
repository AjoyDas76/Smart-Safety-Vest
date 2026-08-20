# Phase 03 - MPU6050 Motion Detection

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

The objective of this phase is to establish communication between the ESP32 and the MPU6050 sensor and acquire real-time motion data. This phase serves as the foundation for implementing motion analysis and fall detection in later stages of the Smart Safety Vest project.

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

## Output Parameters

### Accelerometer

- X-axis
- Y-axis
- Z-axis

### Gyroscope

- X-axis
- Y-axis
- Z-axis

---

## Sample Output

```text
========================================
Smart Safety Vest
Phase 03 - MPU6050 Motion Detection
========================================

Initializing MPU6050...

MPU6050 Initialization Completed.

Raw Sensor Data Streaming...

Sample : 1

Accelerometer

ACC X : 124
ACC Y : -56
ACC Z : 16382

Gyroscope

GYRO X : 3
GYRO Y : -1
GYRO Z : 0

========================================
```

---

## Project Notes

The connected MPU6050-compatible module returns a WHO_AM_I value of **0x70** instead of the standard **0x68**.

Because of this hardware behavior, the `testConnection()` function is not used. The sensor communication has been verified through I2C scanning and successful acquisition of raw motion data.

---

## Result

- ESP32 successfully communicates with the MPU6050 sensor.
- Accelerometer data is received successfully.
- Gyroscope data is received successfully.
- The sensor is ready for motion analysis.

---

## Next Phase

**Phase 03.2 – Sensor Calibration & Tilt Angle Calculation**

The next phase will include:

- Accelerometer calibration
- Gyroscope calibration
- Roll angle calculation
- Pitch angle calculation
- Motion stability analysis

These features will prepare the system for implementing the fall detection algorithm.

---

## GitHub Commit

```
Phase 03 Completed: Implemented MPU6050 raw motion sensing with stable initialization and real-time accelerometer/gyroscope data streaming (v1.1)
```
