#include <TinyGPS++.h>

TinyGPSPlus gps;

enum WorkerStatus
{
  STANDING,
  WALKING,
  RUNNING,
  FAST_MOVEMENT
};

WorkerStatus workerStatus = STANDING;

// Counter for stable detection
int walkingCount = 0;
int runningCount = 0;
int standingCount = 0;

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("Worker Speed Detection (Final)");
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
      float speed = gps.speed.kmph();

      // -------- Stable Detection --------

      if (speed < 1.5)
      {
        standingCount++;
        walkingCount = 0;
        runningCount = 0;

        if (standingCount >= 3)
        {
          workerStatus = STANDING;
        }
      }

      else if (speed < 5.0)
      {
        walkingCount++;
        standingCount = 0;
        runningCount = 0;

        if (walkingCount >= 3)
        {
          workerStatus = WALKING;
        }
      }

      else if (speed < 12.0)
      {
        runningCount++;
        walkingCount = 0;
        standingCount = 0;

        if (runningCount >= 3)
        {
          workerStatus = RUNNING;
        }
      }

      else
      {
        workerStatus = FAST_MOVEMENT;

        standingCount = 0;
        walkingCount = 0;
        runningCount = 0;
      }

      // ---------- Display ----------

      Serial.println("--------------------------------");

      Serial.print("Latitude : ");
      Serial.println(gps.location.lat(), 6);

      Serial.print("Longitude : ");
      Serial.println(gps.location.lng(), 6);

      Serial.print("Speed : ");
      Serial.print(speed);
      Serial.println(" km/h");

      Serial.print("Worker Status : ");

      switch(workerStatus)
      {
        case STANDING:
          Serial.println("STANDING");
          break;

        case WALKING:
          Serial.println("WALKING");
          break;

        case RUNNING:
          Serial.println("RUNNING");
          break;

        case FAST_MOVEMENT:
          Serial.println("FAST MOVEMENT");
          break;
      }
    }
    else
    {
      Serial.println("Waiting for GPS Fix...");
    }
  }
}