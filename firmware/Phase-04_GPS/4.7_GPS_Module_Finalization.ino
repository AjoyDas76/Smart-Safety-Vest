#include <TinyGPS++.h>

// ================================================================
//  PHASE 4 — GPS TRACKING SYSTEM (FINAL MODULE)
//  Combines: Integration, Lat/Lng Reading, Accuracy Validation,
//            Status Monitoring, Distance Calculation, Optimization
// ================================================================

TinyGPSPlus gps;

// ---------- Status ----------

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

// ---------- Accuracy ----------

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

// ---------- Distance ----------

double startLat = 0;
double startLng = 0;

double previousLat = 0;
double previousLng = 0;

double totalDistance = 0;

bool firstFix = false;

const double DRIFT_FILTER = 2.0;

// ---------- Optimization ----------

const int FILTER_SIZE = 5;

double latBuffer[FILTER_SIZE];
double lngBuffer[FILTER_SIZE];

int bufferIndex = 0;
int bufferCount = 0;

// ---------- GPS Data Struct ----------

struct GPSData
{
  bool fixValid;
  double latitude;
  double longitude;
  double altitude;
  double speed;
  int satellites;
  double hdop;
  double distanceFromStart;
  double totalDistance;
  GPSStatus status;
  AccuracyLevel accuracy;
};

GPSData gpsData;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  startTime = millis();

  Serial.println("GPS Module Finalization Started...");
}

void loop()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  updateStatus();
  updateAccuracy();
  updateGPSData();

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 2000)
  {
    lastPrint = millis();
    printGPSData();
  }
}

// ---------- Status Update ----------

void updateStatus()
{
  if (millis() - startTime < 3000)
  {
    gpsStatus = INITIALIZING;
    return;
  }

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

// ---------- Accuracy Update ----------

void updateAccuracy()
{
  if (!gps.location.isValid() || !gps.hdop.isValid())
  {
    accuracyLevel = INVALID;
    return;
  }

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

// ---------- GPS Data Update (Distance + Optimization) ----------

void updateGPSData()
{
  gpsData.fixValid = gps.location.isValid();
  gpsData.satellites = gps.satellites.value();
  gpsData.hdop = gps.hdop.isValid() ? gps.hdop.hdop() : -1;
  gpsData.altitude = gps.altitude.isValid() ? gps.altitude.meters() : -1;
  gpsData.speed = gps.speed.isValid() ? gps.speed.kmph() : -1;
  gpsData.status = gpsStatus;
  gpsData.accuracy = accuracyLevel;

  if (!gpsData.fixValid)
    return;

  double rawLat = gps.location.lat();
  double rawLng = gps.location.lng();

  // Moving average filter (jitter reduction)
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

  gpsData.latitude = smoothLat;
  gpsData.longitude = smoothLng;

  // Distance tracking
  if (!firstFix)
  {
    startLat = smoothLat;
    startLng = smoothLng;

    previousLat = smoothLat;
    previousLng = smoothLng;

    firstFix = true;

    Serial.println("Start Position Saved");
  }

  double currentDistance =
  TinyGPSPlus::distanceBetween(
  previousLat,
  previousLng,
  smoothLat,
  smoothLng);

  if (currentDistance > DRIFT_FILTER)
  {
    totalDistance += currentDistance;

    previousLat = smoothLat;
    previousLng = smoothLng;
  }

  gpsData.distanceFromStart =
  TinyGPSPlus::distanceBetween(
  startLat,
  startLng,
  smoothLat,
  smoothLng);

  gpsData.totalDistance = totalDistance;
}

// ---------- Display ----------

void printGPSData()
{
  Serial.println("--------------------------------");

  Serial.print("GPS Status : ");

  switch (gpsData.status)
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

  Serial.print("Accuracy : ");

  switch (gpsData.accuracy)
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

  Serial.print("Satellites : ");
  Serial.println(gpsData.satellites);

  Serial.print("HDOP : ");
  if (gpsData.hdop >= 0)
    Serial.println(gpsData.hdop);
  else
    Serial.println("N/A");

  Serial.print("Latitude : ");
  if (gpsData.fixValid)
    Serial.println(gpsData.latitude, 6);
  else
    Serial.println("N/A");

  Serial.print("Longitude : ");
  if (gpsData.fixValid)
    Serial.println(gpsData.longitude, 6);
  else
    Serial.println("N/A");

  Serial.print("Altitude : ");
  if (gpsData.altitude >= 0)
  {
    Serial.print(gpsData.altitude);
    Serial.println(" m");
  }
  else
    Serial.println("N/A");

  Serial.print("Speed : ");
  if (gpsData.speed >= 0)
  {
    Serial.print(gpsData.speed);
    Serial.println(" km/h");
  }
  else
    Serial.println("N/A");

  Serial.print("Distance From Start : ");
  if (gpsData.fixValid)
  {
    Serial.print(gpsData.distanceFromStart);
    Serial.println(" m");
  }
  else
    Serial.println("N/A");

  Serial.print("Total Distance : ");
  Serial.print(gpsData.totalDistance);
  Serial.println(" m");
}
