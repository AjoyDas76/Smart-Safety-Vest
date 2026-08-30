/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.8_IoT_Module_Finalization.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - Module Finalization
 * Author  : Ajoy Das Team
 *
 * Description:
 * Production-ready IoT cloud module combining all of 6.1-6.7:
 *   - WiFi with auto-reconnect
 *   - Firebase Auth + TLS
 *   - Live sensor upload (BME280)
 *   - Live GPS upload
 *   - Fall alert upload (MPU6050)
 *   - Full telemetry storage + history
 *   - Secure, device-scoped writes
 *
 * This is the cloud side of the vest used by the mobile app
 * (Phase 7) and full integration (Phase 9).
 *
 * Credentials in Firebase_Config.h.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include "Firebase_Config.h"

Adafruit_BME280 bme;
MPU6050 mpu;
TinyGPSPlus gps;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DEVICE_ID "vest-01"
#define TELEMETRY_INTERVAL_MS 10000
#define HISTORY_INTERVAL_MS   60000

unsigned long lastTelemetry = 0;
unsigned long lastHistory = 0;

// ---------- MPU6050 fall detection ----------
const int FILTER_SIZE = 10;
long axBuf[FILTER_SIZE] = {0};
long ayBuf[FILTER_SIZE] = {0};
long azBuf[FILTER_SIZE] = {0};
int filterIndex = 0;

int16_t ax, ay, az;
int16_t gx, gy, gz;
float axF, ayF, azF;
float pitch, roll, refPitch = 0, refRoll = 0, tiltPitch, tiltRoll;

const float FREE_FALL_THRESHOLD = 0.80;
const float IMPACT_THRESHOLD    = 1.8;
const float LYING_ANGLE         = 70.0;
const unsigned long LYING_CONFIRM_TIME = 1500;
const unsigned long FALL_TIMEOUT       = 6000;

bool freeFallDetected = false;
bool impactDetected   = false;
bool lyingDetected    = false;
bool fallLatched      = false;
unsigned long fallStartTime  = 0;
unsigned long lyingStartTime = 0;
bool lastFallSent = false;

void updateMPUFilter()
{
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  axBuf[filterIndex] = ax;
  ayBuf[filterIndex] = ay;
  azBuf[filterIndex] = az;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;

  long sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < FILTER_SIZE; i++)
  {
    sx += axBuf[i]; sy += ayBuf[i]; sz += azBuf[i];
  }

  axF = sx / (float)FILTER_SIZE;
  ayF = sy / (float)FILTER_SIZE;
  azF = sz / (float)FILTER_SIZE;

  pitch = atan2(azF, sqrt(axF*axF + ayF*ayF)) * 180.0 / PI;
  roll  = atan2(ayF, sqrt(axF*axF + azF*azF)) * 180.0 / PI;

  tiltPitch = pitch - refPitch;
  tiltRoll  = roll  - refRoll;
}

float totalAccelerationG()
{
  return sqrt((float)ax*ax + (float)ay*ay + (float)az*az) / 16384.0;
}

bool updateFallDetection()
{
  if (totalAccelerationG() < FREE_FALL_THRESHOLD)
  {
    freeFallDetected = true;
    if (fallStartTime == 0) fallStartTime = millis();
  }

  if (totalAccelerationG() > IMPACT_THRESHOLD) impactDetected = true;

  bool horizontal = (fabs(tiltPitch) > LYING_ANGLE || fabs(tiltRoll) > LYING_ANGLE);
  if (horizontal)
  {
    if (lyingStartTime == 0) lyingStartTime = millis();
    if (millis() - lyingStartTime >= LYING_CONFIRM_TIME) lyingDetected = true;
  }
  else
  {
    lyingDetected = false;
    lyingStartTime = 0;
  }

  if (fallLatched)
  {
    if (fabs(tiltPitch) < 10 && fabs(tiltRoll) < 10) fallLatched = false;
    return fallLatched;
  }

  if (fallStartTime != 0 && millis() - fallStartTime <= FALL_TIMEOUT)
  {
    if (freeFallDetected && impactDetected && lyingDetected)
    {
      fallLatched = true;
      fallStartTime = 0;
      return true;
    }
  }
  else
  {
    fallStartTime = 0;
    freeFallDetected = false;
    impactDetected = false;
  }

  return false;
}

// ---------- Connectivity ----------
void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED && millis() < 20000)
  {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());
}

void initFirebase()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.reconnectWiFi(true);
  fbdo.setBSSLBufferSize(4096, 1024);

  Firebase.begin(&config, &auth);
}

// ---------- Uploads ----------
void uploadTelemetry()
{
  if (!Firebase.ready()) return;

  String dev = String(F("/devices/")) + DEVICE_ID;

  float temperature = bme.readTemperature();
  float humidity    = bme.readHumidity();
  float pressure    = bme.readPressure() / 100.0F;

  Firebase.setFloat(fbdo, (dev + "/sensors/temperature").c_str(), temperature);
  Firebase.setFloat(fbdo, (dev + "/sensors/humidity").c_str(), humidity);
  Firebase.setFloat(fbdo, (dev + "/sensors/pressure").c_str(), pressure);

  if (gps.location.isValid())
  {
    Firebase.setDouble(fbdo, (dev + "/location/lat").c_str(), gps.location.lat());
    Firebase.setDouble(fbdo, (dev + "/location/lng").c_str(), gps.location.lng());
    Firebase.setFloat(fbdo, (dev + "/location/speed").c_str(),
                           gps.speed.isValid() ? gps.speed.kmph() : 0);
  }

  Firebase.setString(fbdo, (dev + "/status/state").c_str(),
                          fallLatched ? "FALL" : "NORMAL");
  Firebase.setInt(fbdo, (dev + "/status/updatedAt").c_str(), millis() / 1000);

  Serial.println(F("[IoT] Telemetry uploaded."));
}

void uploadFallAlert()
{
  if (!Firebase.ready()) return;

  double lat = gps.location.isValid() ? gps.location.lat() : 0;
  double lng = gps.location.isValid() ? gps.location.lng() : 0;

  String alerts = String(F("/devices/")) + DEVICE_ID + String(F("/alerts"));

  Firebase.setBool(fbdo, (alerts + "/fall").c_str(), true);
  Firebase.setDouble(fbdo, (alerts + "/lat").c_str(), lat);
  Firebase.setDouble(fbdo, (alerts + "/lng").c_str(), lng);
  Firebase.setInt(fbdo, (alerts + "/timestamp").c_str(), millis() / 1000);
  Firebase.setString(fbdo, "/alerts/latest", DEVICE_ID);

  Serial.println(F("!!! [IoT] FALL ALERT UPLOADED !!!"));
}

void uploadHistory()
{
  if (!Firebase.ready()) return;

  String path = String(F("/history/")) + DEVICE_ID + "/" +
                String(millis() / 1000);

  Firebase.setFloat(fbdo, (path + "/temperature").c_str(), bme.readTemperature());
  Firebase.setFloat(fbdo, (path + "/humidity").c_str(), bme.readHumidity());
  Firebase.setFloat(fbdo, (path + "/pressure").c_str(), bme.readPressure() / 100.0F);

  Serial.println(F("[IoT] History record written."));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.8: IoT Module Finalization ==="));

  Wire.begin(21, 22);
  mpu.initialize();

  if (!bme.begin(0x76))
  {
    Serial.println(F("BME280 not found at 0x76, trying 0x77"));
    bme.begin(0x77);
  }

  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  connectWiFi();
  initFirebase();

  Serial.println(F("IoT module ready. Streaming all telemetry..."));
}

void loop()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  updateMPUFilter();
  bool isFall = updateFallDetection();

  if (isFall && !lastFallSent)
  {
    lastFallSent = true;
    uploadFallAlert();
  }

  if (!isFall) lastFallSent = false;

  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS)
  {
    lastTelemetry = millis();
    uploadTelemetry();
  }

  if (millis() - lastHistory >= HISTORY_INTERVAL_MS)
  {
    lastHistory = millis();
    uploadHistory();
  }
}
