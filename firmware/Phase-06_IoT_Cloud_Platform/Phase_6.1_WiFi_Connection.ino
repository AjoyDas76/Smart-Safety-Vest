/*
 * Project : Smart Safety Vest
 * Firmware: Phase_6.1_WiFi_Connection.ino
 * Version : v1.0
 * Module  : IoT Cloud Platform - WiFi Connection
 * Author  : Ajoy Das Team
 *
 * Description:
 * Connect the vest to the local WiFi network with automatic
 * reconnection handling. This is the foundation for all cloud
 * uploads. Includes:
 *   - Non-blocking WiFi.begin() with status tracking
 *   - Automatic reconnect on disconnection
 *   - Connection quality reporting (RSSI)
 *
 * Note: The vest is usually paired with a mobile hotspot
 * (phone) on site. Configure WIFI_SSID / WIFI_PASSWORD below.
 */

#include <WiFi.h>

#define WIFI_SSID     "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 10000;

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(F("Connecting to WiFi: "));
  Serial.println(WIFI_SSID);
}

void printWiFiStatus()
{
  Serial.println("--------------------------------");

  Serial.print("WiFi Status    : ");

  switch (WiFi.status())
  {
    case WL_CONNECTED:
      Serial.println("CONNECTED");
      break;
    case WL_IDLE_STATUS:
      Serial.println("IDLE");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("NO SSID AVAILABLE");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("CONNECT FAILED");
      break;
    case WL_CONNECTION_LOST:
      Serial.println("CONNECTION LOST");
      break;
    case WL_DISCONNECTED:
      Serial.println("DISCONNECTED");
      break;
    default:
      Serial.println("UNKNOWN");
      break;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("IP Address     : ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal (RSSI)  : ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    Serial.print("Gateway        : ");
    Serial.println(WiFi.gatewayIP());
  }
}

void handleWiFiReconnect()
{
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastReconnectAttempt >= RECONNECT_INTERVAL)
  {
    lastReconnectAttempt = millis();
    Serial.println(F("WiFi lost, reconnecting..."));
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 6.1: WiFi Connection ==="));

  connectWiFi();
}

void loop()
{
  handleWiFiReconnect();

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 5000)
  {
    lastPrint = millis();
    printWiFiStatus();
  }
}
