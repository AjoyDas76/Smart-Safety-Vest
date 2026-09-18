# Vest Command v1.3

**LoRa-based Smart Industrial Safety Vest Monitoring System** — a command-center app for real-time worker telemetry, live GPS tracking, sensor alerts, and reporting.

## 🚨 Live Telemetry & Fall Detection
- Automatic **fall detection** using accelerometer and gyroscope data — instant alert if a worker falls
- Live **motion state** tracking (Walking / Running / Idle)
- Monitoring of temperature, humidity, and toxic gas levels — critical alarm when thresholds are exceeded
- **SOS button** on the vest triggers an immediate audio-visual emergency siren
- Distinct color coding per sensor: Temperature (Red/Coral), Humidity (Cyan), Pressure (Purple)

## 🗺️ GPS Map & Clean UI
- Live position tracking of workers on Google Maps
- One-tap switch between Street view and Satellite/Hybrid view
- All map controls consolidated into a single **unified floating toolbar**
- **Collapsible bottom HUD**, minimal by default (54dp), expandable to a full coordinates panel with a tap

## 🚧 Geofencing & Voice Warning
- Create custom **danger zones** on the map via long-press or coordinates (high voltage, excavation, chemical hazard, etc.)
- **Proximity alert** within 50 meters, with distance shown
- Automatic **TTS voice warning** when approaching or entering a danger zone
- Color-coded zone status: Safe (green), Approaching (yellow/orange), Breach (blinking red)

## 📍 Last Known Location & Offline Rescue
- Automatic caching of the last known location and timestamp when signal is lost
- **Time-lapse indicator** (e.g. "Lost 4m ago")
- One-click **COPY GPS** and **NAVIGATE** (launches Google Maps navigation)

## 🧭 Breadcrumbs Trail
- Historical route trail drawn on the map showing where a worker has walked during a shift
- Toggle trail on/off, and clear trail history with a single tap

## 📊 Dashboard & Reporting
- **Dashboard** — overview and status of all workers at a glance
- **Live Chart** — real-time graphs of sensor data
- **Alerts** — list of all active and past alerts
- **Worker Profiles** — individual worker profiles with worker/batch ID
- **Report** — shift-based summary reporting
- **Safety Rules** — on-site safety guideline reference
- **Settings** — app configuration

## 🔐 Login & Onboarding
- Secure login screen
- Branded splash screen

## 🔧 Technical
- Built with Kotlin + Jetpack Compose
- Supports Android 7.0 (API 24) and above, targetSdk 36
- Push notification support

---

*Note: This release note was compiled from the project's feature report and the app's screen/manager modules. Please review the Dashboard, Live Chart, Report, Safety Rules, Worker Profiles, and Settings sections yourself before publishing, since their exact in-app details were inferred from file names rather than manually verified.*
