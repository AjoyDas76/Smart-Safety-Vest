# Smart Vest Command — Android App (Kotlin)

Native Android Studio project for the Smart Safety Vest real-time monitoring app,
built to match your web dashboard: glassmorphism UI, Google Maps GPS tracking,
and MPAndroidChart live sensor charts.

## How to open in Android Studio

1. Unzip this folder anywhere on your computer.
2. Open Android Studio → **File > Open** → select the unzipped `SmartVestCommand` folder.
3. Let Gradle sync (first sync needs internet — it downloads the Gradle wrapper,
   Google Maps SDK, MPAndroidChart, and other dependencies automatically).
4. Plug in a device or start an emulator **with Google Play services** (MPAndroidChart
   works on any emulator, but Google Maps requires a Play Store image).
5. Click Run ▶️.

If Android Studio asks about the Gradle wrapper jar, choose "Use Gradle from: wrapper"
and let it regenerate the wrapper — this is normal for a hand-created project.

## What's included

| Screen | File(s) |
|---|---|
| Splash (3s, then routes to Login/Dashboard) | `ui/SplashActivity.kt` + `activity_splash.xml` |
| Login (frosted glass over your uploaded background image) | `ui/LoginActivity.kt` + `activity_login.xml` |
| Persistent VEST COMMAND bar + drawer navigation | `ui/MainActivity.kt` + `activity_main.xml` |
| Dashboard (Worker Status + Temp/Humidity/Pressure/Active Alerts) | `ui/DashboardFragment.kt` |
| Live Chart (3 live line charts) | `ui/LiveChartFragment.kt` |
| Map Tracking (fullscreen Google Map + ❎ close) | `ui/MapTrackingFragment.kt` |
| Alert (active alerts list) | `ui/AlertsFragment.kt` |
| Report (Daily/Weekly/Monthly tabs + bar chart) | `ui/ReportFragment.kt` |
| Settings (Theme/Audio/Notification toggles + Logout) | `ui/SettingsFragment.kt` |

## Wiring up real hardware data

Right now, sensor values, GPS coordinates, and alerts are **simulated** with random
numbers so the app is fully runnable and demoable out of the box. Search for the
`TODO` and "replace this simulation" comments in:

- `DashboardFragment.kt` — live temperature/humidity/pressure/alert count
- `LiveChartFragment.kt` — the 3 live charts
- `MapTrackingFragment.kt` — GPS position updates
- `AlertsFragment.kt` — the alerts list
- `ReportFragment.kt` — daily/weekly/monthly report data
- `LoginActivity.kt` — currently accepts any non-empty username/password; wire this
  to your real authentication backend

Swap those simulated blocks for your real data source (BLE from the vest, a
WebSocket/MQTT feed, or a REST API) — the UI update calls are already in place.

## Google Maps API key

Your key is already wired into `AndroidManifest.xml` via `strings.xml`
(`google_maps_key`). Two things worth doing before you publish:

1. In Google Cloud Console, restrict the key to your app's package name
   (`com.smartvest.command`) and SHA-1 signing certificate, and to only the
   "Maps SDK for Android" API.
2. Don't commit `strings.xml` with a live key to a public repo — consider moving
   it to `local.properties` (gitignored) if you plan to open-source this later.

## Theme

The Light/Dark switch in Settings restyles the whole app (backgrounds, glass
cards, drawer, text) live via `util/ThemeManager.kt` and `util/PrefsManager.kt`.
Both light and dark glass palettes were pulled directly from your website's CSS
variables so the look matches.
