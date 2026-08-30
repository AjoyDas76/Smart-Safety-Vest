/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.4_Live_GPS_Upload.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - Live GPS Upload
 * Author  : Ajoy Das Team
 *
 * Description:
 * Upload live GPS location to Firebase Realtime Database so the
 * base station / mobile app can track the worker in real time.
 * Uses the accuracy gate from Phase 4.3 and smoothing from 4.8.
 *
 * Database path:
 *   /devices/<deviceId>/location/{lat,lng,speed,accuracy,timestamp}
 *
 * Credentials in Firebase_Config.h.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <TinyGPS++.h>
#include "Firebase_Config.h"

TinyGPSPlus gps;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DEVICE_ID "vest-01"
#define UPLOAD_INTERVAL_MS 10000

#define MAX_HDOP       3.0
#define MIN_SATELLITES 4

unsigned long lastUpload = 0;

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

bool fixIsAcceptable()
{
  if (!gps.location.isValid()) return false;
  if (gps.satellites.value() < MIN_SATELLITES) return false;
  if (gps.hdop.isValid() && gps.hdop.hdop() > MAX_HDOP) return false;
  return true;
}

void uploadLocation()
{
  if (!Firebase.ready()) return;
  if (!fixIsAcceptable()) return;

  double lat = gps.location.lat();
  double lng = gps.location.lng();
  float speed = gps.speed.isValid() ? gps.speed.kmph() : 0.0;

  String base = String(F("/devices/")) + DEVICE_ID + String(F("/location"));

  Firebase.setDouble(fbdo, (base + "/lat").c_str(), lat);
  Firebase.setDouble(fbdo, (base + "/lng").c_str(), lng);
  Firebase.setFloat(fbdo, (base + "/speed").c_str(), speed);
  Firebase.setInt(fbdo, (base + "/satellites").c_str(), gps.satellites.value());
  Firebase.setInt(fbdo, (base + "/timestamp").c_str(), millis() / 1000);

  Serial.print(F("Uploaded location: "));
  Serial.print(lat, 6);
  Serial.print(F(", "));
  Serial.print(lng, 6);
  Serial.print(F(" @ "));
  Serial.print(speed, 1);
  Serial.println(F(" km/h"));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.4: Live GPS Upload ==="));

  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  connectWiFi();
  initFirebase();

  Serial.println(F("Streaming GPS to Firebase..."));
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
    uploadLocation();
  }
}
