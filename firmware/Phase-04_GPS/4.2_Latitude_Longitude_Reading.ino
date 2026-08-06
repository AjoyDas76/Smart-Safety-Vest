#include <TinyGPS++.h>

TinyGPSPlus gps;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("GPS Reading Started...");
}

void loop()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 2000)
  {
    lastPrint = millis();

    Serial.println("--------------------------------");

    Serial.print("Satellites : ");
    Serial.println(gps.satellites.value());

    Serial.print("HDOP : ");
    Serial.println(gps.hdop.hdop());

    Serial.print("Fix : ");
    Serial.println(gps.location.isValid() ? "YES" : "NO");

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

    Serial.print("Altitude : ");
    if (gps.altitude.isValid())
    {
      Serial.print(gps.altitude.meters());
      Serial.println(" m");
    }
    else
      Serial.println("N/A");

    Serial.print("Speed : ");
    if (gps.speed.isValid())
    {
      Serial.print(gps.speed.kmph());
      Serial.println(" km/h");
    }
    else
      Serial.println("N/A");

    Serial.print("Date : ");
    if (gps.date.isValid())
    {
      Serial.print(gps.date.day());
      Serial.print("/");
      Serial.print(gps.date.month());
      Serial.print("/");
      Serial.println(gps.date.year());
    }
    else
      Serial.println("N/A");

    Serial.print("Time : ");
    if (gps.time.isValid())
    {
      Serial.print(gps.time.hour());
      Serial.print(":");
      Serial.print(gps.time.minute());
      Serial.print(":");
      Serial.println(gps.time.second());
    }
    else
      Serial.println("N/A");
  }
}