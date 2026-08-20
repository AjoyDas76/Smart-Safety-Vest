/*
 * Project : Smart Safety Vest
 * Phase   : Phase 03.3 - Moving Average Filter
 * File    : Phase03_Moving_Average_Filter_v1.0.ino
 * Author  : Ajoy Das Team
 * Description:
 * Applies a 10-sample moving average filter to
 * MPU6050 accelerometer data.
 */

#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

const uint8_t FILTER_SIZE = 10;

int16_t axBuffer[FILTER_SIZE];
int16_t ayBuffer[FILTER_SIZE];
int16_t azBuffer[FILTER_SIZE];

uint8_t indexBuffer = 0;

int16_t ax, ay, az;
int16_t gx, gy, gz;

void setup()
{
  Serial.begin(115200);
  Wire.begin(21,22);

  mpu.initialize();

  // Buffer Initialize
  for(int i=0;i<FILTER_SIZE;i++)
  {
    axBuffer[i]=0;
    ayBuffer[i]=0;
    azBuffer[i]=0;
  }

  Serial.println("Moving Average Filter Started");
}

void loop()
{
  mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz);

  axBuffer[indexBuffer]=ax;
  ayBuffer[indexBuffer]=ay;
  azBuffer[indexBuffer]=az;

  indexBuffer++;

  if(indexBuffer>=FILTER_SIZE)
      indexBuffer=0;

  long axSum=0;
  long aySum=0;
  long azSum=0;

  for(int i=0;i<FILTER_SIZE;i++)
  {
      axSum+=axBuffer[i];
      aySum+=ayBuffer[i];
      azSum+=azBuffer[i];
  }

  int16_t axFiltered=axSum/FILTER_SIZE;
  int16_t ayFiltered=aySum/FILTER_SIZE;
  int16_t azFiltered=azSum/FILTER_SIZE;

  Serial.println("--------------------------------");

  Serial.print("RAW AX : ");
  Serial.print(ax);

  Serial.print("   FILTER : ");
  Serial.println(axFiltered);

  Serial.print("RAW AY : ");
  Serial.print(ay);

  Serial.print("   FILTER : ");
  Serial.println(ayFiltered);

  Serial.print("RAW AZ : ");
  Serial.print(az);

  Serial.print("   FILTER : ");
  Serial.println(azFiltered);

  delay(100);
}