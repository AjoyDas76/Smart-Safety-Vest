#include <TinyGPS++.h>

TinyGPSPlus gps;

const int FILTER_SIZE = 5;

double latBuffer[FILTER_SIZE];
double lngBuffer[FILTER_SIZE];

int bufferIndex = 0;
int bufferCount = 0;

double lastSentLat = 0;
double lastSentLng = 0;

bool firstFix = false;

const double MIN_DISTANCE_CHANGE = 2.0;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("GPS Data Optimization Started...");
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
      double rawLat = gps.location.lat();
      double rawLng = gps.location.lng();

      // ---------- Moving Average Filter ----------

      latBuffer[bufferIndex] = rawLat;
      lngBuffer[bufferIndex] = rawLng;

      bufferIndex = (bufferIndex + 1) % FILTER_SIZE;

      if (bufferCount < FILTER_SIZE)
        bufferCount++;

      double smoothLat = 0;
      double smoothLng = 0;

      for (int i = 0; i < bufferCount; i++)
      {
        smoothLat += latBuffer[i];
        smoothLng += lngBuffer[i];
      }

      smoothLat /= bufferCount;
      smoothLng /= bufferCount;

      // ---------- Send-On-Change Optimization ----------

      if (!firstFix)
      {
        lastSentLat = smoothLat;
        lastSentLng = smoothLng;

        firstFix = true;

        Serial.println("--------------------------------");
        Serial.println("Initial Position Locked");
      }

      double movedDistance =
      TinyGPSPlus::distanceBetween(
      lastSentLat,
      lastSentLng,
      smoothLat,
      smoothLng);

      Serial.println("--------------------------------");

      Serial.print("Raw Latitude : ");
      Serial.println(rawLat, 6);

      Serial.print("Raw Longitude : ");
      Serial.println(rawLng, 6);

      Serial.print("Filtered Latitude : ");
      Serial.println(smoothLat, 6);

      Serial.print("Filtered Longitude : ");
      Serial.println(smoothLng, 6);

      if (movedDistance >= MIN_DISTANCE_CHANGE)
      {
        lastSentLat = smoothLat;
        lastSentLng = smoothLng;

        Serial.print("Data Sent (Moved ");
        Serial.print(movedDistance);
        Serial.println(" m)");
      }
      else
      {
        Serial.println("Data Skipped (No Significant Movement)");
      }
    }
    else
    {
      Serial.println("Waiting for GPS Fix...");
    }
  }
}
