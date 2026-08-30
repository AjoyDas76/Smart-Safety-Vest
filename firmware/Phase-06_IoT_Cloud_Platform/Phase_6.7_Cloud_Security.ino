/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.7_Cloud_Security.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - Cloud Security
 * Author  : Ajoy Das Team
 *
 * Description:
 * Demonstrates secure cloud access practices for the vest:
 *   - TLS connection (Firebase uses HTTPS by default)
 *   - Email/password authentication (never embed the API key
 *     alone; always authenticate with a real user)
 *   - Device-scoped database writes (each vest only writes
 *     under /devices/<deviceId>)
 *   - Secure token refresh handling
 *   - Local secret management guidance (see notes below)
 *
 * SECURITY NOTES (IMPORTANT):
 *   1. NEVER commit real credentials. This repository contains
 *      placeholders only. Real values live in your local
 *      Firebase_Config.h which is git-ignored.
 *   2. Use Firebase Realtime Database Rules to restrict access:
 *        {
 *          "rules": {
 *            "devices": {
 *              "$deviceId": {
 *                ".read": "auth != null",
 *                ".write": "auth != null && auth.uid == $deviceId"
 *              }
 *            },
 *            "alerts": { ".read": "auth != null", ".write": "auth != null" }
 *          }
 *        }
 *   3. Rotate the vest user password regularly.
 *   4. Enable SecurityRules testing in the Firebase console.
 *
 * Credentials in Firebase_Config.h.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include "Firebase_Config.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DEVICE_ID "vest-01"

unsigned long lastReconnect = 0;
bool lastSignedIn = false;

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
  // TLS is used automatically (HTTPS database URL).
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.reconnectWiFi(true);
  fbdo.setBSSLBufferSize(4096, 1024);

  Firebase.begin(&config, &auth);
}

void checkSecureStatus()
{
  if (Firebase.ready())
  {
    if (!lastSignedIn)
    {
      lastSignedIn = true;
      Serial.println(F("[SECURITY] Authenticated with Firebase Auth."));
      Serial.print(F("[SECURITY] Token UID: "));
      Serial.println(auth.token.uid.c_str());
    }

    // Scoped write: only under /devices/<deviceId>/security
    // Realtime Database rules reject writes outside this path.
    String scopedPath = String(F("/devices/")) + DEVICE_ID +
                        String(F("/security/lastCheck"));

    if (Firebase.setInt(fbdo, (scopedPath).c_str(), millis() / 1000))
    {
      Serial.print(F("[SECURITY] Scoped write OK: "));
      Serial.println(scopedPath);
    }
    else
    {
      Serial.print(F("[SECURITY] Write denied or failed: "));
      Serial.println(fbdo.errorReason());
    }
  }
  else
  {
    lastSignedIn = false;
    Serial.println(F("[SECURITY] Not authenticated yet."));
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.7: Cloud Security ==="));

  connectWiFi();
  initFirebase();

  Serial.println(F("Security checks active."));
}

void loop()
{
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck >= 5000)
  {
    lastCheck = millis();
    checkSecureStatus();
  }
}
