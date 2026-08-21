#include <TinyGPS++.h>

TinyGPSPlus gps;

enum AccuracyLevel
{
  EXCELLENT,
  GOOD,
  MODERATE,
  POOR,
  INVALID
};

AccuracyLevel accuracyLevel = INVALID;

const int MIN_SATELLITES = 4;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("GPS Accuracy Validation Started...");
}

void loop()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  // ---------- Accuracy Logic ----------

  if (!gps.location.isValid() || !gps.hdop.isValid())
  {
    accuracyLevel = INVALID;
  }
  else
  {
    double hdopValue = gps.hdop.hdop();

    if (gps.satellites.value() < MIN_SATELLITES)
    {
      accuracyLevel = POOR;
    }
    else if (hdopValue <= 1.0)
    {
      accuracyLevel = EXCELLENT;
    }
    else if (hdopValue <= 2.0)
    {
      accuracyLevel = GOOD;
    }
    else if (hdopValue <= 5.0)
    {
      accuracyLevel = MODERATE;
    }
    else
    {
      accuracyLevel = POOR;
    }
  }

  // ---------- Display ----------

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 2000)
  {
    lastPrint = millis();

    Serial.println("--------------------------------");

    Serial.print("Satellites : ");
    Serial.println(gps.satellites.value());

    Serial.print("HDOP : ");
    if (gps.hdop.isValid())
      Serial.println(gps.hdop.hdop());
    else
      Serial.println("N/A");

    Serial.print("Accuracy : ");

    switch (accuracyLevel)
    {
      case EXCELLENT:
        Serial.println("EXCELLENT");
        break;

      case GOOD:
        Serial.println("GOOD");
        break;

      case MODERATE:
        Serial.println("MODERATE");
        break;

      case POOR:
        Serial.println("POOR");
        break;

      case INVALID:
        Serial.println("INVALID");
        break;
    }

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
