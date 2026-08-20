/*
 * Project : Smart Safety Vest
 * Phase   : Phase 03.4 - Pitch & Roll Angle Calculation
 * File    : Phase03_Angle_Calculation.ino
 * Author  : Ajoy Das Team
 * Description:
 * Reads MPU6050 raw accelerometer data,
 * applies Moving Average Filter,
 * calculates Pitch & Roll angles,
 * and displays filtered sensor values.
 */

#include <Wire.h>
#include <MPU6050.h>
#include <math.h>

MPU6050 mpu;

// Raw Values
int16_t ax, ay, az;
int16_t gx, gy, gz;

// Moving Average
const int FILTER_SIZE = 10;

long axBuffer[FILTER_SIZE];
long ayBuffer[FILTER_SIZE];
long azBuffer[FILTER_SIZE];

int indexFilter = 0;

float axFiltered = 0;
float ayFiltered = 0;
float azFiltered = 0;

// Angles
float pitch;
float roll;

void setup()
{
  Serial.begin(115200);

  Wire.begin(21, 22);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Smart Safety Vest");
  Serial.println(" Phase 03.4 - Angle Calculation");
  Serial.println("========================================");

  mpu.initialize();

  Serial.println("MPU6050 Connected Successfully");
  Serial.println("Moving Average Filter Started");
  Serial.println();
}

void loop()
{
  // Read Sensor
  mpu.getMotion6(&ax, &ay, &az,
                 &gx, &gy, &gz);

  // Store New Sample
  axBuffer[indexFilter] = ax;
  ayBuffer[indexFilter] = ay;
  azBuffer[indexFilter] = az;

  indexFilter++;

  if (indexFilter >= FILTER_SIZE)
    indexFilter = 0;

  // Calculate Average
  long sumAX = 0;
  long sumAY = 0;
  long sumAZ = 0;

  for (int i = 0; i < FILTER_SIZE; i++)
  {
    sumAX += axBuffer[i];
    sumAY += ayBuffer[i];
    sumAZ += azBuffer[i];
  }

  axFiltered = sumAX / (float)FILTER_SIZE;
  ayFiltered = sumAY / (float)FILTER_SIZE;
  azFiltered = sumAZ / (float)FILTER_SIZE;

  // Angle Calculation
  pitch = atan2(
              axFiltered,
              sqrt(ayFiltered * ayFiltered +
                   azFiltered * azFiltered))
          * 180.0 / PI;

  roll = atan2(
             ayFiltered,
             sqrt(axFiltered * axFiltered +
                  azFiltered * azFiltered))
         * 180.0 / PI;

  Serial.println("----------------------------------------");

  Serial.print("AX : ");
  Serial.print(axFiltered);

  Serial.print("   AY : ");
  Serial.print(ayFiltered);

  Serial.print("   AZ : ");
  Serial.println(azFiltered);

  Serial.print("Pitch : ");
  Serial.print(pitch, 2);
  Serial.println(" deg");

  Serial.print("Roll  : ");
  Serial.print(roll, 2);
  Serial.println(" deg");

  Serial.println("----------------------------------------");

  delay(200);
}