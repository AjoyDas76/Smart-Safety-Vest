# Smart Safety Vest — Project Roadmap (v2)

> পরিবর্তনের কারণ: আগে ওয়ার্কারের মুভমেন্ট (STANDING/WALKING/RUNNING) GPS স্পিড দিয়ে
> ডিটেক্ট করার পরিকল্পনা ছিল (Phase 4.6, 4.7)। কিন্তু GPS স্পিড হাঁটার গতির মতো ধীর
> মুভমেন্টে যথেষ্ট নয়েজি ও ল্যাগি, তাই STANDING বনাম WALKING নির্ভরযোগ্যভাবে আলাদা করা
> যাচ্ছিল না। এখন এই দায়িত্ব সম্পূর্ণভাবে MPU6050-এ (Phase 3) স্থানান্তরিত হয়েছে —
> Accelerometer/Gyroscope দিয়ে posture (lean/lying/fall) ও motion-energy (walking/
> running) দুটোই একসাথে পরিচালিত হয়। GPS এখন শুধুই বিশুদ্ধ লোকেশন রিপোর্টার (Phase 4)।
>
> **শুধুমাত্র Phase 3 ও Phase 4 পরিবর্তিত হয়েছে। বাকি সব Phase অপরিবর্তিত রাখা হয়েছে।**

---

## PHASE 1 — PROJECT INITIALIZATION
1.1 Project Planning
1.2 Hardware Selection
1.3 ESP32 Development Setup
1.4 Repository Initialization

## PHASE 2 — ENVIRONMENTAL MONITORING (BME280)
2.1 BME280 Integration
2.2 Temperature Reading
2.3 Humidity Reading
2.4 Atmospheric Pressure Reading
2.5 Data Filtering & Averaging
2.6 Environmental Monitoring Module

## PHASE 3 — MPU6050 MOTION, ACTIVITY & FALL DETECTION *(updated)*
3.1 MPU6050 Integration
3.2 Motion Detection (Lean)
3.3 Free Fall Detection
3.4 Impact Detection
3.5 Lying Detection
3.6 Recovery Detection
3.7 Fall Decision Engine
3.8 Fall Latching System
3.9 **Worker Movement Detection (Standing / Walking / Running)** — *new: moved from GPS-speed-based (old 4.6/4.7) to MPU6050 gyroscope motion-energy based*
3.10 **Posture-Gated Movement Correction** — *new: prevents false WALKING/RUNNING while the worker is leaning/bent; movement is only evaluated when posture is upright (STANDING)*
3.11 Motion Module Testing & Bug Fix
3.12 MPU6050 Module Finalization

## PHASE 4 — GPS TRACKING SYSTEM *(updated — location-only)*
4.1 GPS Module Integration
4.2 Latitude & Longitude Reading
4.3 GPS Accuracy Validation
4.4 GPS Status Monitoring
4.5 Distance Calculation *(re-scoped: site boundary / geofencing / travel-distance logging — no longer used for movement or speed classification)*
4.6 GPS Data Optimization
4.7 GPS Module Finalization

> ~~4.6 Worker Speed Detection~~ এবং ~~4.7 Worker Movement Detection~~ (পুরাতন রোডম্যাপ) —
> সরিয়ে ফেলা হয়েছে; এই কাজ এখন Phase 3.9–3.10-এ MPU6050 দিয়ে হচ্ছে।

## PHASE 5 — LoRa COMMUNICATION SYSTEM (SX1278 RA-02)
5.1 SX1278 RA-02 Integration
5.2 LoRa Transmitter Setup (Safety Vest)
5.3 LoRa Receiver Setup (Base Station)
5.4 Worker Status Transmission
5.5 GPS Data Transmission
5.6 Fall Alert Transmission
5.7 Communication Range Testing
5.8 Packet Reliability & Error Handling
5.9 LoRa Communication Module Finalization

## PHASE 6 — IoT CLOUD PLATFORM
6.1 WiFi Connection
6.2 Firebase Integration
6.3 Live Sensor Upload
6.4 Live GPS Upload
6.5 Fall Alert Upload
6.6 Data Storage & Dashboard
6.7 Cloud Security
6.8 IoT Module Finalization

## PHASE 7 — MOBILE APPLICATION
7.1 User Authentication
7.2 Dashboard (Live Data)
7.3 Live Worker Status
7.4 Live GPS Tracking
7.5 Emergency Alerts
7.6 Notification System
7.7 App Testing & Optimization
7.8 Mobile App Finalization

## PHASE 8 — SMART SAFETY FEATURES
8.1 SOS Button
8.2 Buzzer Alarm
8.3 LED Warning Indicator
8.4 Battery Monitoring
8.5 Power Management
8.6 Low Battery Alert
8.7 Safety Module Finalization

## PHASE 9 — SYSTEM INTEGRATION
9.1 Sensor Integration
9.2 GPS + LoRa Integration
9.3 Firebase + Mobile App Integration
9.4 End-to-End Testing
9.5 Performance Optimization
9.6 System Stability Testing
9.7 Integration Finalization

## PHASE 10 — FINAL PRODUCT RELEASE
- Field Testing
- Performance Evaluation
- Documentation
- Thesis Preparation
- GitHub Final Release
- Smart Safety Vest v1.0
