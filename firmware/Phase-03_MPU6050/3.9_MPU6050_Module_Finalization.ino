/*
 * Project : Smart Safety Vest
 * Phase   : Phase 03.9 - MPU6050 Module Finalization
 * File    : Phase03_MPU6050_Module_Finalization.ino
 * Author  : Ajoy Das Team
 * Description:
 * Final, consolidated MPU6050 module. Combines Reference/Auto
 * Calibration, Moving Average Filter, Pitch & Roll Angle
 * Calculation, Motion State, Fall Decision Engine and Worker
 * Movement Detection (Standing / Walking / Running) into a
 * single clean, struct-based interface ready for integration
 * with the rest of the Smart Safety Vest firmware (GPS module,
 * alert system, etc).
 *
 * Reporting priority every loop (highest wins):
 *   FALL_DETECTED  >  LYING  >  LEAN (F/B/L/R)  >  WALKING/RUNNING/STANDING
 *
 * Board: ESP32
 * Wiring:
 *   MPU6050: SDA -> GPIO21, SCL -> GPIO22 (I2C)
 */

#include <Wire.h>
#include <MPU6050.h>
#include <math.h>

MPU6050 mpu;

/*
 * ==========================================================
 * Raw + Filtered Sensor Data
 * ==========================================================
 */

const int FILTER_SIZE = 10;
const int CAL_SAMPLES = 100;

long axBuf[FILTER_SIZE] = {0};
long ayBuf[FILTER_SIZE] = {0};
long azBuf[FILTER_SIZE] = {0};
int filterIndex = 0;

int16_t ax, ay, az;
int16_t gx, gy, gz;

float axF, ayF, azF;

float pitch, roll;
float refPitch, refRoll;
float tiltPitch, tiltRoll;

float totalAcceleration = 0.0;  // filtered accel magnitude (lean use)
float rawAcceleration    = 0.0; // raw accel magnitude (free-fall/impact use)

/*
 * ==========================================================
 * Posture / Lean Thresholds
 * ==========================================================
 */

float pitchThreshold = 15.0;
float rollThreshold  = 20.0;

/*
 * ==========================================================
 * Free Fall Detection
 * ==========================================================
 */

bool freeFallDetected = false;
const float FREE_FALL_THRESHOLD = 0.80;

/*
 * ==========================================================
 * Impact Detection
 * ==========================================================
 */

bool impactDetected = false;
const float IMPACT_THRESHOLD = 1.8;

/*
 * ==========================================================
 * Lying Position Detection
 * ==========================================================
 */

bool lyingDetected = false;
unsigned long lyingStartTime = 0;
const unsigned long LYING_CONFIRM_TIME = 1500; // must hold 1.5s
const float LYING_ANGLE = 70.0;

/*
 * ==========================================================
 * Fall Latching
 * ==========================================================
 */

bool fallDetected = false;
bool fallLatched  = false;
bool workerRecovered = false;

unsigned long fallStartTime = 0;
const unsigned long FALL_TIMEOUT = 6000;

/*
 * ==========================================================
 * Posture State Machine (STANDING / LEAN / LYING / FALL)
 * ==========================================================
 */

enum PostureState
{
  POSTURE_STANDING = 0,
  POSTURE_FORWARD_LEAN,
  POSTURE_BACKWARD_LEAN,
  POSTURE_LEFT_LEAN,
  POSTURE_RIGHT_LEAN,
  POSTURE_LYING,
  POSTURE_FALL
};

PostureState postureState = POSTURE_STANDING;
PostureState previousPostureState = POSTURE_STANDING;
bool postureChanged = false;

// True only when posture is upright (not leaning/lying/fallen) - gates
// the movement classifier below.
bool isUpright = true;

/*
 * ==========================================================
 * Movement-Energy Classifier
 * Only allowed to run while isUpright == true
 * ==========================================================
 */

const int ENERGY_WINDOW = 15;
float energyBuf[ENERGY_WINDOW] = {0};
int energyIndex = 0;
bool energyBufFull = false;

float STANDING_ENERGY_MAX = 1500.0;
float WALKING_ENERGY_MAX  = 6000.0;
// anything above WALKING_ENERGY_MAX (while upright) is classified RUNNING

enum MovementState
{
  MOVEMENT_STANDING = 0,
  MOVEMENT_WALKING,
  MOVEMENT_RUNNING
};

MovementState movementState = MOVEMENT_STANDING;

// Debounce - 3 consecutive samples before switching state
int standingCount = 0;
int walkingCount  = 0;
int runningCount  = 0;

/*
 * ==========================================================
 * Final MPU Data Structure
 * (single struct exposed to the rest of the firmware)
 * ==========================================================
 */

struct MPUData
{
  float tiltPitch;
  float tiltRoll;
  float accelerationG;
  float motionEnergy;

  bool isUpright;
  bool freeFall;
  bool impact;
  bool lying;
  bool fall;

  PostureState posture;
  MovementState movement;

  const char* status;
};

MPUData mpuData;

/*
 * ==========================================================
 * MPU update: reads sensor once, computes everything both
 * posture logic and movement logic need from that one read.
 * ==========================================================
 */

void updateMPUFilter()
{
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  axBuf[filterIndex] = ax;
  ayBuf[filterIndex] = ay;
  azBuf[filterIndex] = az;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;

  long sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < FILTER_SIZE; i++) { sx += axBuf[i]; sy += ayBuf[i]; sz += azBuf[i]; }

  axF = sx / (float)FILTER_SIZE;
  ayF = sy / (float)FILTER_SIZE;
  azF = sz / (float)FILTER_SIZE;

  pitch = atan2(azF, sqrt(axF * axF + ayF * ayF)) * 180.0 / PI;
  roll  = atan2(ayF, sqrt(axF * axF + azF * azF)) * 180.0 / PI;

  totalAcceleration = sqrt(axF * axF + ayF * ayF + azF * azF);
  rawAcceleration = sqrt((float)ax * ax + (float)ay * ay + (float)az * az);

  // ---- rolling gyro motion-energy ----
  float instantEnergy = fabs(gx) + fabs(gy) + fabs(gz);
  energyBuf[energyIndex] = instantEnergy;
  energyIndex = (energyIndex + 1) % ENERGY_WINDOW;
  if (energyIndex == 0) energyBufFull = true;
}

float getAverageEnergy()
{
  int count = energyBufFull ? ENERGY_WINDOW : energyIndex;
  if (count == 0) return 0;

  float sum = 0;
  for (int i = 0; i < count; i++) sum += energyBuf[i];
  return sum / count;
}

/*
 * ==========================================================
 * Free Fall / Impact / Lying Detection
 * ==========================================================
 */

void detectFreeFall()
{
  float normalizedAcceleration = rawAcceleration / 16384.0;

  if (normalizedAcceleration < FREE_FALL_THRESHOLD)
  {
    freeFallDetected = true;
    if (fallStartTime == 0) fallStartTime = millis();
  }
}

void detectImpact()
{
  float normalizedAcceleration = rawAcceleration / 16384.0;

  if (normalizedAcceleration > IMPACT_THRESHOLD)
  {
    impactDetected = true;
  }
}

void detectLyingPosition()
{
  bool horizontal = (fabs(tiltPitch) > LYING_ANGLE || fabs(tiltRoll) > LYING_ANGLE);

  if (horizontal)
  {
    if (lyingStartTime == 0) lyingStartTime = millis();

    if (millis() - lyingStartTime >= LYING_CONFIRM_TIME)
    {
      lyingDetected = true;
    }
  }
  else
  {
    lyingDetected = false;
    lyingStartTime = 0;
  }
}

void detectRecovery()
{
  workerRecovered = (!lyingDetected && fabs(tiltPitch) < 10 && fabs(tiltRoll) < 10);
}

/*
 * ==========================================================
 * Posture state machine (STANDING vs LEAN vs LYING vs FALL)
 * This decides isUpright, which gates the movement classifier.
 * ==========================================================
 */

void updatePostureState()
{
  if (fallLatched)
  {
    postureState = POSTURE_FALL;
    isUpright = false;
    postureChanged = (previousPostureState != postureState);
    previousPostureState = postureState;
    return;
  }

  // Lying has higher priority than lean
  if (lyingDetected)
  {
    postureState = POSTURE_LYING;
    isUpright = false;
    postureChanged = (previousPostureState != postureState);
    previousPostureState = postureState;
    return;
  }

  // Default: upright
  postureState = POSTURE_STANDING;

  if (tiltPitch < -pitchThreshold)
  {
    postureState = POSTURE_FORWARD_LEAN;
  }
  else if (tiltPitch > pitchThreshold)
  {
    postureState = POSTURE_BACKWARD_LEAN;
  }
  else if (tiltRoll > rollThreshold)
  {
    postureState = POSTURE_LEFT_LEAN;
  }
  else if (tiltRoll < -rollThreshold)
  {
    postureState = POSTURE_RIGHT_LEAN;
  }

  isUpright = (postureState == POSTURE_STANDING);

  postureChanged = (previousPostureState != postureState);
  previousPostureState = postureState;
}

/*
 * ==========================================================
 * Movement classification.
 * Only updates WALKING/RUNNING while isUpright == true. While
 * leaning/lying/fallen, debounce counters are reset and
 * movementState is forced back to STANDING so no stale
 * WALKING/RUNNING carries over once the worker straightens up.
 * ==========================================================
 */

void updateMovementState()
{
  if (!isUpright)
  {
    standingCount = 0;
    walkingCount  = 0;
    runningCount  = 0;
    movementState = MOVEMENT_STANDING;
    return;
  }

  float avgEnergy = getAverageEnergy();

  if (avgEnergy < STANDING_ENERGY_MAX)
  {
    standingCount++; walkingCount = 0; runningCount = 0;
    if (standingCount >= 3) movementState = MOVEMENT_STANDING;
  }
  else if (avgEnergy < WALKING_ENERGY_MAX)
  {
    walkingCount++; standingCount = 0; runningCount = 0;
    if (walkingCount >= 3) movementState = MOVEMENT_WALKING;
  }
  else
  {
    runningCount++; walkingCount = 0; standingCount = 0;
    if (runningCount >= 3) movementState = MOVEMENT_RUNNING;
  }
}

/*
 * ==========================================================
 * Fall Latching (auto-recovery once the worker stands back up)
 * ==========================================================
 */

void updateFallDetection()
{
  if (fallLatched)
  {
    if (!workerRecovered)
    {
      fallDetected = true;
      return;
    }

    // Worker সত্যিই দাঁড়িয়েছে
    fallLatched = false;
    fallDetected = false;
    freeFallDetected = false;
    impactDetected = false;
    // NOTE: lyingDetected intentionally NOT reset here.
    // detectLyingPosition() is the single owner of this flag.
    fallStartTime = 0;
  }

  if (fallStartTime != 0 && millis() - fallStartTime <= FALL_TIMEOUT)
  {
    if (!fallLatched && freeFallDetected && impactDetected && lyingDetected)
    {
      fallDetected = true;
      fallLatched = true;
      fallStartTime = 0;
    }
    else if (!fallLatched)
    {
      fallDetected = false;
    }
  }
  else
  {
    fallDetected = false;
    fallStartTime = 0;
    freeFallDetected = false;
    impactDetected = false;
    // NOTE: lyingDetected intentionally NOT reset here.
  }
}

/*
 * ==========================================================
 * Final worker status label - priority:
 * FALL > LYING > LEAN(F/B/L/R) > WALKING/RUNNING/STANDING
 * ==========================================================
 */

const char* getFinalWorkerStatus()
{
  switch (postureState)
  {
    case POSTURE_FALL:           return "FALL DETECTED";
    case POSTURE_LYING:          return "LYING";
    case POSTURE_FORWARD_LEAN:   return "FORWARD LEAN";
    case POSTURE_BACKWARD_LEAN:  return "BACKWARD LEAN";
    case POSTURE_LEFT_LEAN:      return "LEFT LEAN";
    case POSTURE_RIGHT_LEAN:     return "RIGHT LEAN";
    case POSTURE_STANDING:
    default:
      switch (movementState)
      {
        case MOVEMENT_WALKING: return "WALKING";
        case MOVEMENT_RUNNING: return "RUNNING";
        case MOVEMENT_STANDING:
        default:                return "STANDING";
      }
  }
}

/*
 * ==========================================================
 * Fill Final MPUData Struct
 * ==========================================================
 */

void updateMPUData()
{
  mpuData.tiltPitch     = tiltPitch;
  mpuData.tiltRoll      = tiltRoll;
  mpuData.accelerationG = rawAcceleration / 16384.0;
  mpuData.motionEnergy  = getAverageEnergy();

  mpuData.isUpright = isUpright;
  mpuData.freeFall  = freeFallDetected;
  mpuData.impact    = impactDetected;
  mpuData.lying     = lyingDetected;
  mpuData.fall      = fallDetected;

  mpuData.posture  = postureState;
  mpuData.movement = movementState;

  mpuData.status = getFinalWorkerStatus();
}

/*
 * ==========================================================
 * Calibration (Reference + Auto Calibration)
 * ==========================================================
 */

void calibrateMPU()
{
  Serial.println("Automatic Calibration - keep the worker still...");
  for (int i = 5; i > 0; i--) { Serial.println(i); delay(1000); }

  float ps = 0, rs = 0;
  for (int i = 0; i < CAL_SAMPLES; i++)
  {
    updateMPUFilter();
    ps += pitch; rs += roll;
    delay(20);
  }

  refPitch = ps / CAL_SAMPLES;
  refRoll  = rs / CAL_SAMPLES;

  Serial.print("Reference Pitch: "); Serial.println(refPitch, 2);
  Serial.print("Reference Roll: ");  Serial.println(refRoll, 2);

  // Warm up the energy buffer while standing still
  for (int i = 0; i < ENERGY_WINDOW * 2; i++)
  {
    updateMPUFilter();
    delay(20);
  }
}

/*
 * ==========================================================
 * Module Init
 * ==========================================================
 */

void initMPU()
{
  Wire.begin(21, 22);
  mpu.initialize();
  calibrateMPU();
}

/*
 * ==========================================================
 * Display
 * ==========================================================
 */

void printMPUData()
{
  Serial.println("--------------------------------");

  Serial.print("Worker Status : ");
  Serial.println(mpuData.status);

  Serial.print("Upright (gate): ");
  Serial.println(mpuData.isUpright ? "YES" : "NO");

  Serial.print("Motion Energy (avg) : ");
  Serial.println(mpuData.motionEnergy, 0);

  Serial.print("TiltPitch: "); Serial.print(mpuData.tiltPitch, 2);
  Serial.print(" TiltRoll: "); Serial.println(mpuData.tiltRoll, 2);

  Serial.print("FreeFall: "); Serial.print(mpuData.freeFall ? "YES" : "NO");
  Serial.print(" Impact: "); Serial.print(mpuData.impact ? "YES" : "NO");
  Serial.print(" Lying: "); Serial.println(mpuData.lying ? "YES" : "NO");

  Serial.print("Fall: ");
  Serial.println(mpuData.fall ? "YES" : "NO");

  Serial.print("Acceleration G: ");
  Serial.println(mpuData.accelerationG, 2);
}

void setup()
{
  Serial.begin(115200);

  initMPU();

  Serial.println("MPU6050 Module Finalization Started...");
}

void loop()
{
  updateMPUFilter();

  tiltPitch = pitch - refPitch;
  tiltRoll  = roll  - refRoll;

  detectFreeFall();
  detectImpact();
  detectLyingPosition();
  detectRecovery();

  updatePostureState();   // decides isUpright (the gate)
  updateMovementState();  // WALKING/RUNNING only evaluated if isUpright
  updateFallDetection();

  updateMPUData();

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 2000)
  {
    lastPrint = millis();
    printMPUData();
  }

  delay(20); // keep MPU sample rate steady for the energy window
}
