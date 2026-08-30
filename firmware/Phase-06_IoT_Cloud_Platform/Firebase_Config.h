/*
 * Project : Smart Safety Vest
 * File     : Firebase_Config.h
 * Version  : v1.0
 * Author   : Ajoy Das Team
 *
 * Shared Firebase / WiFi credentials for the IoT Cloud phase.
 *
 * HOW TO CONFIGURE:
 *   1. Copy this file and edit the values below.
 *   2. NEVER commit real credentials to the repository.
 *   3. Provide the values through your own Firebase console.
 *
 * Steps to get these values:
 *   - API_KEY:    Firebase console -> Project settings -> General
 *                 -> Web API Key
 *   - DATABASE_URL: Firebase console -> Realtime Database ->
 *                 copy the database URL
 *   - USER_EMAIL / USER_PASSWORD: Firebase console ->
 *                 Authentication -> Users -> add a user that
 *                 the vest will authenticate as
 */

#ifndef FIREBASE_CONFIG_H
#define FIREBASE_CONFIG_H

// ---------- WiFi ----------
#define WIFI_SSID     "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

// ---------- Firebase ----------
#define API_KEY           "YourFirebaseAPIKey"
#define DATABASE_URL      "https://your-project-default-rtdb.firebaseio.com"
#define USER_EMAIL        "vest@example.com"
#define USER_PASSWORD     "YourFirebaseUserPassword"

#endif
