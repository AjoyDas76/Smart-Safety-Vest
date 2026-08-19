#include <TinyGPS++.h>

TinyGPSPlus gps;

double previousLat = 0;
double previousLng = 0;

double startLat = 0;
double startLng = 0;

double totalDistance = 0;

bool firstFix = false;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("GPS Distance Calculation");
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

    if (gps.location.isValid())
    {
      double lat = gps.location.lat();
      double lng = gps.location.lng();

      if (!firstFix)
      {
        startLat = lat;
        startLng = lng;

        previousLat = lat;
        previousLng = lng;

        firstFix = true;

        Serial.println("Start Position Saved");
      }

      double currentDistance =
      TinyGPSPlus::distanceBetween(
      previousLat,
      previousLng,
      lat,
      lng);

      // GPS Drift Filter
      if (currentDistance > 2.0)
      {
        totalDistance += currentDistance;

        previousLat = lat;
        previousLng = lng;
      }

      double distanceFromStart =
      TinyGPSPlus::distanceBetween(
      startLat,
      startLng,
      lat,
      lng);

      Serial.println("--------------------------------");

      Serial.print("Latitude : ");
      Serial.println(lat, 6);

      Serial.print("Longitude : ");
      Serial.println(lng, 6);

      Serial.print("Distance From Start : ");
      Serial.print(distanceFromStart);
      Serial.println(" m");

      Serial.print("Total Distance : ");
      Serial.print(totalDistance);
      Serial.println(" m");
    }
    else
    {
      Serial.println("Waiting for GPS Fix...");
    }
  }
}