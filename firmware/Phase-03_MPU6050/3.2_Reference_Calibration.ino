/*
 * Project : Smart Safety Vest
 * Phase   : Phase 03.2 - Reference Calibration
 * File    : Phase03_Reference_Calibration_v1.0.ino
 * Author  : Ajoy Das Team
 * Description:
 * Automatically captures the worker's normal standing
 * position at startup and stores it as the reference
 * position for future motion and fall detection.
 */

#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

//==============================
// Motion Data Structure
//==============================
struct MotionData
{
  int16_t ax;
  int16_t ay;
  int16_t az;

  int16_t gx;
  int16_t gy;
  int16_t gz;
};

MotionData reference;
MotionData current;

//==============================
// Sample Configuration
//==============================
const uint16_t SAMPLE_COUNT = 500;

void captureReference()
{
  long axSum = 0;
  long aySum = 0;
  long azSum = 0;

  long gxSum = 0;
  long gySum = 0;
  long gzSum = 0;

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Stand Still");
  Serial.println(" Capturing Reference Position...");
  Serial.println("========================================");

  for (uint16_t i = 0; i < SAMPLE_COUNT; i++)
  {
    mpu.getMotion6(
      &current.ax,
      &current.ay,
      &current.az,
      &current.gx,
      &current.gy,
      &current.gz);

    axSum += current.ax;
    aySum += current.ay;
    azSum += current.az;

    gxSum += current.gx;
    gySum += current.gy;
    gzSum += current.gz;

    delay(5);
  }

  reference.ax = axSum / SAMPLE_COUNT;
  reference.ay = aySum / SAMPLE_COUNT;
  reference.az = azSum / SAMPLE_COUNT;

  reference.gx = gxSum / SAMPLE_COUNT;
  reference.gy = gySum / SAMPLE_COUNT;
  reference.gz = gzSum / SAMPLE_COUNT;

  Serial.println();
  Serial.println("Reference Position Saved Successfully");
  Serial.println();

  Serial.println("Reference Values");

  Serial.print("AX : ");
  Serial.println(reference.ax);

  Serial.print("AY : ");
  Serial.println(reference.ay);

  Serial.print("AZ : ");
  Serial.println(reference.az);

  Serial.print("GX : ");
  Serial.println(reference.gx);

  Serial.print("GY : ");
  Serial.println(reference.gy);

  Serial.print("GZ : ");
  Serial.println(reference.gz);

  Serial.println();
  Serial.println("System Ready");
  Serial.println("========================================");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Smart Safety Vest");
  Serial.println(" Phase 03.2 - Reference Calibration");
  Serial.println("========================================");

  Serial.println("Initializing MPU6050...");
  mpu.initialize();

  delay(500);

  Serial.println();
  Serial.println("Please wear the vest.");
  Serial.println("Stand normally.");
  Serial.println();

  for (int i = 3; i >= 1; i--)
  {
    Serial.print("Calibration starts in ");
    Serial.println(i);
    delay(1000);
  }

  captureReference();
}

void loop()
{
  mpu.getMotion6(
      &current.ax,
      &current.ay,
      &current.az,
      &current.gx,
      &current.gy,
      &current.gz);

  Serial.print("dAX : ");
  Serial.print(current.ax - reference.ax);

  Serial.print("   dAY : ");
  Serial.print(current.ay - reference.ay);

  Serial.print("   dAZ : ");
  Serial.println(current.az - reference.az);

  delay(200);
}