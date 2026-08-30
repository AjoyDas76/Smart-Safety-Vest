/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.6_Data_Storage_Dashboard.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - Data Storage & Dashboard
 * Author  : Ajoy Das Team
 *
 * Description:
 * Uploads complete worker telemetry to Firebase with a proper
 * data schema for storage, and manages historical records:
 *
 *   /devices/<deviceId>/
 *     status/       { state, updatedAt }
 *     sensors/      { temperature, humidity, pressure }
 *     location/     { lat, lng, speed, accuracy }
 *     battery/      { percent, voltage }
 *     alerts/       { fall, sos, lat, lng, timestamp }
 *   /history/       timestamped records for dashboards
 *
 * Also handles the periodic "session" records so the mobile
 * dashboard can render charts and history.
 *
 * Credentials in Firebase_Config.h.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TinyGPS++.h>
#include "Firebase_Config.h"

Adafruit_BME280 bme;
TinyGPSPlus gps;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DEVICE_ID "vest-01"
#define UPLOAD_INTERVAL_MS 15000
#define HISTORY_INTERVAL_MS 60000

unsigned long lastUpload = 0;
unsigned long lastHistory = 0;
unsigned long sessionId = 1;

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
  Firebase.begin(&config, &auth);
}

void uploadTelemetry()
{
  if (!Firebase.ready()) return;

  String dev = String(F("/devices/")) + DEVICE_ID;

  // ---- Sensor data ----
  float temperature = bme.readTemperature();
  float humidity    = bme.readHumidity();
  float pressure    = bme.readPressure() / 100.0F;

  Firebase.setFloat(fbdo, (dev + "/sensors/temperature").c_str(), temperature);
  Firebase.setFloat(fbdo, (dev + "/sensors/humidity").c_str(), humidity);
  Firebase.setFloat(fbdo, (dev + "/sensors/pressure").c_str(), pressure);

  // ---- Location ----
  if (gps.location.isValid())
  {
    Firebase.setDouble(fbdo, (dev + "/location/lat").c_str(), gps.location.lat());
    Firebase.setDouble(fbdo, (dev + "/location/lng").c_str(), gps.location.lng());
    Firebase.setFloat(fbdo, (dev + "/location/speed").c_str(),
                           gps.speed.isValid() ? gps.speed.kmph() : 0);
    Firebase.setInt(fbdo, (dev + "/location/satellites").c_str(),
                         gps.satellites.value());
  }

  // ---- Status ----
  Firebase.setString(fbdo, (dev + "/status/state").c_str(), "NORMAL");
  Firebase.setInt(fbdo, (dev + "/status/updatedAt").c_str(), millis() / 1000);

  Serial.println(F("Telemetry stored to Firebase."));
}

void uploadHistory()
{
  if (!Firebase.ready()) return;

  // timestamped history record for charts
  String path = String(F("/history/")) + DEVICE_ID + "/" +
                String(millis() / 1000);

  Firebase.setFloat(fbdo, (path + "/temperature").c_str(), bme.readTemperature());
  Firebase.setFloat(fbdo, (path + "/humidity").c_str(), bme.readHumidity());
  Firebase.setFloat(fbdo, (path + "/pressure").c_str(),
                         bme.readPressure() / 100.0F);

  if (gps.location.isValid())
  {
    Firebase.setDouble(fbdo, (path + "/lat").c_str(), gps.location.lat());
    Firebase.setDouble(fbdo, (path + "/lng").c_str(), gps.location.lng());
  }

  sessionId++;
  Firebase.setInt(fbdo,
      String(F("/devices/")) + DEVICE_ID + "/session/id", sessionId);

  Serial.println(F("History record written."));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.6: Data Storage & Dashboard ==="));

  if (!bme.begin(0x76))
  {
    Serial.println(F("BME280 not found!"));
    while (1);
  }

  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  connectWiFi();
  initFirebase();

  Serial.println(F("Storing telemetry to Firebase..."));
}

void loop()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  if (millis() - lastUpload >= UPLOAD_INTERVAL_MS)
  {
    lastUpload = millis();
    uploadTelemetry();
  }

  if (millis() - lastHistory >= HISTORY_INTERVAL_MS)
  {
    lastHistory = millis();
    uploadHistory();
  }
}
