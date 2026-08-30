# Phase-06 IoT Cloud Platform

## Objective

Connect the Smart Safety Vest to the internet via WiFi and Google Firebase so worker telemetry is available in real time to supervisors. The cloud platform stores sensor data, GPS location, and emergency alerts, and feeds the Phase 7 mobile application.

## Components

- ESP32 DevKit V1
- BME280 (temperature / humidity / pressure)
- MPU6050 (fall detection)
- NEO-M8N GPS
- WiFi / Mobile Hotspot
- Google Firebase account (Realtime Database)

## Architecture

```
Vest ESP32 (WiFi)  -->  Firebase Realtime Database  -->  Mobile App / Dashboard
     |                         /devices/<deviceId>/ ...
     +-- BME280  (sensors)
     +-- MPU6050 (fall alert)
     +-- GPS     (location)
```

## Code / Sub-steps

| Step | File | Description |
|------|------|-------------|
| 6.1 | `Phase_6.1_WiFi_Connection.ino` | WiFi connect + auto-reconnect + RSSI reporting |
| 6.2 | `Phase_6.2_Firebase_Integration.ino` | Firebase Auth + DB read/write verification |
| 6.3 | `Phase_6.3_Live_Sensor_Upload.ino` | Live BME280 data upload |
| 6.4 | `Phase_6.4_Live_GPS_Upload.ino` | Live GPS location upload |
| 6.5 | `Phase_6.5_Fall_Alert_Upload.ino` | Immediate fall alert upload + location |
| 6.6 | `Phase_6.6_Data_Storage_Dashboard.ino` | Full telemetry storage + history records |
| 6.7 | `Phase_6.7_Cloud_Security.ino` | Auth, device-scoped writes, TLS, rules |
| 6.8 | `Phase_6.8_IoT_Module_Finalization.ino` | **Final**: all cloud features combined |
| - | `Firebase_Config.h` | Shared WiFi/Firebase credential placeholder (never commit real keys) |

## Database Schema

```
/devices/<deviceId>/
  sensors/     temperature, humidity, pressure
  location/    lat, lng, speed, satellites
  status/      state, updatedAt
  alerts/      fall, sos, lat, lng, timestamp
  session/     id
/history/<deviceId>/<timestamp>/
  temperature, humidity, pressure, lat, lng
/alerts/latest   -> deviceId of most recent alert
```

## Firebase Setup

1. Create a project at `console.firebase.google.com`.
2. Enable **Email/Password** authentication and add a vest user.
3. Create a **Realtime Database**.
4. Register an app and copy the **Web API Key**.
5. Fill `Firebase_Config.h` with your WiFi, API key, database URL, and user credentials.

## Security

- Use HTTPS (TLS) - automatic with the Firebase library.
- Authenticate with a real Firebase user (never rely on the API key alone).
- Restrict database rules to authenticated, device-scoped writes. Example rules are documented in `Phase_6.7_Cloud_Security.ino`.

## Test Results

- Firebase Auth signs in and returns a valid token UID.
- All telemetry writes succeed under the device-scoped path.
- Fall alerts appear in the database within the upload interval.

## Notes

- The vest connects through a phone hotspot or site WiFi.
- **Never commit real Firebase credentials** - only placeholders are in this repository.
- Library: `Firebase ESP32 Client` (by Mobizt). This project targets v4.4.x, which
  uses the `FirebaseESP32.h` header and the `Firebase.setXxx(fbdo, path, ...)` API
  (passing `fbdo` by reference, path as `const char*`).
