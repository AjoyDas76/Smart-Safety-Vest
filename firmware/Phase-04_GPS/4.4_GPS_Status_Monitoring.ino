#include <TinyGPS++.h>

TinyGPSPlus gps;

enum GPSStatus
{
  INITIALIZING,
  SEARCHING,
  ACQUIRING_FIX,
  FIXED,
  LOST_FIX
};

GPSStatus gpsStatus = INITIALIZING;

bool previousFix = false;

unsigned long startTime;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  startTime = millis();

  Serial.println("GPS Status Monitoring Started...");
}

void loop()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  // ---------- Status Logic ----------

  if (millis() - startTime < 3000)
  {
    gpsStatus = INITIALIZING;
  }
  else
  {
    bool fix = gps.location.isValid();

    if (fix)
    {
      gpsStatus = FIXED;
      previousFix = true;
    }
    else
    {
      if (previousFix)
      {
        gpsStatus = LOST_FIX;
        previousFix = false;
      }
      else if (gps.satellites.value() == 0)
      {
        gpsStatus = SEARCHING;
      }
      else
      {
        gpsStatus = ACQUIRING_FIX;
      }
    }
  }

  // ---------- Display ----------

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 2000)
  {
    lastPrint = millis();

    Serial.println("--------------------------------");

    Serial.print("GPS Status : ");

    switch (gpsStatus)
    {
      case INITIALIZING:
        Serial.println("INITIALIZING");
        break;

      case SEARCHING:
        Serial.println("SEARCHING");
        break;

      case ACQUIRING_FIX:
        Serial.println("ACQUIRING FIX");
        break;

      case FIXED:
        Serial.println("FIXED");
        break;

      case LOST_FIX:
        Serial.println("LOST FIX");
        break;
    }

    Serial.print("Satellites : ");
    Serial.println(gps.satellites.value());

    Serial.print("HDOP : ");
    Serial.println(gps.hdop.hdop());

    Serial.print("Latitude : ");

    if (gps.location.isValid())
      Serial.println(gps.location.lat(), 6);
    else
      Serial.println("N/A");

    Serial.print("Longitude : ");

    if (gps.location.isValid())
      Serial.println(gps.location.lng(), 6);
    else
      Serial.println("N/A");
  }
}