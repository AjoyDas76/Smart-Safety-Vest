/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.2_Firebase_Integration.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - Firebase Integration
 * Author  : Ajoy Das Team
 *
 * Description:
 * Connect the vest to Google Firebase (Realtime Database) and
 * verify read/write access using email/password authentication.
 * Includes:
 *   - Firebase Auth (email/password) with token management
 *   - Realtime Database connection test
 *   - Signed-in status reporting
 *
 * Firebase setup:
 *   1. Create a project at https://console.firebase.google.com
 *   2. Enable Email/Password sign-in in Authentication
 *   3. Create a Realtime Database
 *   4. Register a web/Android app and copy its API key
 *   5. Add a user in Authentication for the vest to log in as
 *
 * Fill in your credentials below.
 *
 * Library: Firebase ESP32 Client by Mobizt
 *   https://github.com/mobizt/Firebase-ESP-Client
 */

#include <WiFi.h>
#include <FirebaseESP32.h>

// ---------- WiFi ----------
#define WIFI_SSID     "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

// ---------- Firebase ----------
#define API_KEY           "YourFirebaseAPIKey"
#define DATABASE_URL      "https://your-project-default-rtdb.firebaseio.com"
#define USER_EMAIL        "vest@example.com"
#define USER_PASSWORD     "YourFirebaseUserPassword"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastReconnect = 0;
bool firebaseReady = false;

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(F("."));
    if (millis() > 20000) break;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println();
    Serial.print(F("Connected. IP: "));
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println(F("\nWiFi connection failed."));
  }
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

void checkFirebaseStatus()
{
  if (Firebase.ready())
  {
    if (!firebaseReady)
    {
      firebaseReady = true;
      Serial.println(F("Firebase connected & authenticated."));
      Serial.print(F("Auth UID: "));
      Serial.println(auth.token.uid.c_str());
    }

    // Test write access
    if (Firebase.setInt(fbdo,
        "/system/connectionTest",
        millis() % 100000))
    {
      Serial.print(F("DB write OK (test: "));
      Serial.print(fbdo.to<int>());
      Serial.println(F(")"));
    }
    else
    {
      Serial.print(F("DB write failed: "));
      Serial.println(fbdo.errorReason());
    }
  }
  else
  {
    Serial.println(F("Firebase not ready yet..."));
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.2: Firebase Integration ==="));

  connectWiFi();
  initFirebase();
}

void loop()
{
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck >= 5000)
  {
    lastCheck = millis();
    checkFirebaseStatus();
  }
}
