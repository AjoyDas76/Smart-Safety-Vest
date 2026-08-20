/*
 * Project : Smart Safety Vest
 * Phase   : Phase 03 - MPU6050 Motion Detection
 * File    : Phase03_MPU6050_Test_v1.1.ino
 * Author  : Ajoy Das Team
 * Description:
 * Reads raw accelerometer and gyroscope data from the MPU6050
 * sensor using ESP32. This program verifies sensor communication
 * and streams real-time motion data through the Serial Monitor.
 */

#include <Wire.h>
#include <MPU6050.h>

// ==============================
// MPU6050 Object
// ==============================
MPU6050 mpu;

// ==============================
// Accelerometer Data
// ==============================
int16_t ax, ay, az;

// ==============================
// Gyroscope Data
// ==============================
int16_t gx, gy, gz;

// ==============================
// Sample Counter
// ==============================
unsigned long sample = 0;

void setup() {

  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  Serial.println();
  Serial.println("========================================");
  Serial.println("        Smart Safety Vest");
  Serial.println(" Phase 03 - MPU6050 Motion Detection");
  Serial.println("========================================");

  Serial.println("Initializing MPU6050...");

  mpu.initialize();

  delay(100);

  Serial.println("MPU6050 Initialization Completed.");
  Serial.println("Raw Sensor Data Streaming...");
  Serial.println("========================================");
}

void loop() {

  sample++;

  mpu.getMotion6(
    &ax, &ay, &az,
    &gx, &gy, &gz
  );

  Serial.print("Sample : ");
  Serial.println(sample);

  Serial.println();

  // Accelerometer Data
  Serial.println("Accelerometer");
  Serial.print("ACC  X : ");
  Serial.println(ax);

  Serial.print("ACC  Y : ");
  Serial.println(ay);

  Serial.print("ACC  Z : ");
  Serial.println(az);

  Serial.println();

  // Gyroscope Data
  Serial.println("Gyroscope");
  Serial.print("GYRO X : ");
  Serial.println(gx);

  Serial.print("GYRO Y : ");
  Serial.println(gy);

  Serial.print("GYRO Z : ");
  Serial.println(gz);

  Serial.println("========================================");

  delay(500);
}