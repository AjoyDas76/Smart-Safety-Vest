/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.3_Live_Sensor_Upload.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - Live Sensor Upload
 * Author  : Ajoy Das Team
 *
 * Description:
 * Upload live BME280 environmental readings to Firebase
 * Realtime Database. Uses the filtered readings (Phase 2.6).
 * Writes to the path:
 *   /devices/<deviceId>/sensors/{temperature,humidity,pressure}
 *
 * Credentials must be configured in a shared header. See
 * Firebase_Config.h. Fill in your values.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Firebase_Config.h"

Adafruit_BME280 bme;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DEVICE_ID "vest-01"
#define UPLOAD_INTERVAL_MS 10000

unsigned long lastUpload = 0;
unsigned long lastReconnect = 0;

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

void uploadSensors()
{
  if (!Firebase.ready()) return;

  float temperature = bme.readTemperature();
  float humidity    = bme.readHumidity();
  float pressure    = bme.readPressure() / 100.0F;

  String base = String(F("/devices/")) + DEVICE_ID + String(F("/sensors"));

  Firebase.setFloat(fbdo, (base + "/temperature").c_str(), temperature);
  Firebase.setFloat(fbdo, (base + "/humidity").c_str(), humidity);
  Firebase.setFloat(fbdo, (base + "/pressure").c_str(), pressure);
  Firebase.setInt(fbdo, (base + "/timestamp").c_str(), millis() / 1000);

  Serial.print(F("Uploaded sensors: "));
  Serial.print(temperature, 2);
  Serial.print(F(" *C, "));
  Serial.print(humidity, 2);
  Serial.print(F(" %, "));
  Serial.print(pressure, 2);
  Serial.println(F(" hPa"));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.3: Live Sensor Upload ==="));

  if (!bme.begin(0x76))
  {
    Serial.println(F("BME280 not found!"));
    while (1);
  }

  connectWiFi();
  initFirebase();

  Serial.println(F("Streaming sensor data to Firebase..."));
}

void loop()
{
  if (millis() - lastUpload >= UPLOAD_INTERVAL_MS)
  {
    lastUpload = millis();
    uploadSensors();
  }
}
