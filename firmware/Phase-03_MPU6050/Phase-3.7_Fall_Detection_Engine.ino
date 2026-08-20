/*
 * Project : Smart Safety Vest
 * Firmware: Phase-3.7_Fall_Detection_Engine.ino
 * Version : v2.1
 * Module  : Motion State Machine + Fall Detection (Timing Fix)
 * Author  : Ajoy Das Team
 *
#include <Wire.h>
#include <MPU6050.h>
#include <math.h>

MPU6050 mpu; 

/*
 * ==========================================================
 * Worker Motion States
 * ==========================================================
 */

enum WorkerState
{
    STANDING = 0,

    FORWARD_LEAN,
    BACKWARD_LEAN,

    LEFT_LEAN,
    RIGHT_LEAN,

    FALL_DETECTED,

    UNKNOWN_STATE
};

WorkerState currentState = STANDING;

int16_t ax, ay, az;
int16_t gx, gy, gz; 

const int FILTER_SIZE = 10;
const int CAL_SAMPLES = 100;

long axBuf[FILTER_SIZE] = {0};
long ayBuf[FILTER_SIZE] = {0};
long azBuf[FILTER_SIZE] = {0}; 

int filterIndex = 0; 

float axF;
float ayF;
float azF;

float pitch;
float roll;

float refPitch;
float refRoll;

float tiltPitch;
float tiltRoll;

/*
 * ==========================================================
 * Motion Detection Threshold
 * ==========================================================
 */

float pitchThreshold = 15.0;
float rollThreshold  = 20.0;

/*
 * ==========================================================
 * Motion Detection Flag
 * ==========================================================
 */

bool stateChanged = false;

/*
 * ==========================================================
 * Free Fall Detection Variables
 * ==========================================================
 */

bool freeFallDetected = false;

float totalAcceleration = 0.0;
float rawAcceleration = 0.0;

const float FREE_FALL_THRESHOLD = 0.80;

/*
 * ==========================================================
 * Impact Detection Variables
 * ==========================================================
 */

bool impactDetected = false;
const float IMPACT_THRESHOLD = 1.8;

/*
 * ==========================================================
 * Fall Latching
 * ==========================================================
 */

bool fallLatched = false;
bool workerRecovered = false;
/*
 * ==========================================================
 * Lying Position Detection Variables
 * ==========================================================
 */

bool lyingDetected = false;

// Lying Confirmation Timer
unsigned long lyingStartTime = 0;

// Must remain horizontal for 1.5 seconds
const unsigned long LYING_CONFIRM_TIME = 1500;

// Maximum tilt angle considered as lying
const float LYING_ANGLE = 70.0;

/*
 * ==========================================================
 * Fall Decision Variables
 * ==========================================================
 */

bool fallDetected = false;

// Time when free fall started
unsigned long fallStartTime = 0;

// Maximum waiting time after free fall (milliseconds)
const unsigned long FALL_TIMEOUT = 6000;

void updateMPUFilter()
{
  mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz); 
axBuf[filterIndex]=ax; 
ayBuf[filterIndex]=ay; 
azBuf[filterIndex]=az; 

filterIndex = (filterIndex + 1) % FILTER_SIZE;

long sx = 0;
long sy = 0;
long sz = 0; 

for (int i = 0; i < FILTER_SIZE; i++)
{
    sx += axBuf[i];
    sy += ayBuf[i];
    sz += azBuf[i];
}

axF=sx/(float)FILTER_SIZE; 
ayF=sy/(float)FILTER_SIZE; 
azF=sz/(float)FILTER_SIZE; 

pitch = atan2(azF, sqrt(axF*axF + ayF*ayF)) * 180.0 / PI;
roll = atan2(ayF, sqrt(axF*axF + azF*azF)) * 180.0 / PI;

// Filtered Acceleration (Lean Detection)
totalAcceleration =
sqrt(axF * axF + ayF * ayF + azF * azF);

// Raw Acceleration (Impact Detection)
rawAcceleration =
sqrt((float)ax * ax +
     (float)ay * ay +
     (float)az * az);

} 

void calibrate()

{
  
  Serial.println("Automatic Calibration"); 
for(int i=5;i>0;i--)
  {
    Serial.println(i);
  delay(1000);
  
  } 
  
float ps=0,rs=0; 
for(int i=0;i<CAL_SAMPLES;i++)

{
  updateMPUFilter();
ps+=pitch; rs+=roll; 
delay(20);
} 

refPitch=ps/CAL_SAMPLES; 
refRoll=rs/CAL_SAMPLES;

Serial.print("Reference Pitch: ");
Serial.println(refPitch,2); 
Serial.print("Reference Roll: ");
Serial.println(refRoll,2);

/*
 * ==========================================================
 * Update Worker Motion State
 * ==========================================================
 */

} 

/*
 * ==========================================================
 * Free Fall Detection
 * ==========================================================
 */

void detectFreeFall()
{
    float normalizedAcceleration =
rawAcceleration / 16384.0;

if (normalizedAcceleration < FREE_FALL_THRESHOLD)
{
    freeFallDetected = true;

    if(fallStartTime == 0)
    {
        fallStartTime = millis();
    }
}
}

/*
 * ==========================================================
 * Impact Detection
 * ==========================================================
 */

void detectImpact()
{

    float normalizedAcceleration =
        rawAcceleration / 16384.0;

    if(normalizedAcceleration > IMPACT_THRESHOLD)
{
    impactDetected = true;
}
}


/*
 * ==========================================================
 * Lying Position Detection
 * ==========================================================
 */

void detectLyingPosition()
{
    bool horizontal = false;

    if (fabs(tiltPitch) > LYING_ANGLE ||
        fabs(tiltRoll) > LYING_ANGLE)
    {
        horizontal = true;
    }

    if (horizontal)
    {
        // First time entering horizontal position
        if (lyingStartTime == 0)
        {
            lyingStartTime = millis();
        }

        // Stayed horizontal long enough
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
    workerRecovered = false;

    // Worker is standing again
    if(!lyingDetected &&
   fabs(tiltPitch)<10 &&
   fabs(tiltRoll)<10)
{
    workerRecovered=true;
}
else
{
    workerRecovered=false;
}
}

void updateWorkerState()
{

if(fallLatched)
{
    currentState = FALL_DETECTED;
    return;
}
    WorkerState previousState = currentState;

// Lying has higher priority than Lean
if (lyingDetected)
{
    currentState = UNKNOWN_STATE;
    stateChanged = (previousState != currentState);
    return;
}

    // Default State
    currentState = STANDING;

    // Forward Lean
if (tiltPitch < -pitchThreshold)
{
    currentState = FORWARD_LEAN;
}

// Backward Lean
else if (tiltPitch > pitchThreshold)
{
    currentState = BACKWARD_LEAN;
}

// Left Lean
else if (tiltRoll > rollThreshold)
{
    currentState = LEFT_LEAN;
}

// Right Lean
else if (tiltRoll < -rollThreshold)
{
    currentState = RIGHT_LEAN;
}

    // State Change Detection
    if (previousState != currentState)
    {
        stateChanged = true;
    }
    else
    {
        stateChanged = false;
    }
}


/*
 * ==========================================================
 * Print Worker Motion State
 * ==========================================================
 */

/*
 * ==========================================================
 * Final Fall Decision Engine
 * ==========================================================
 */

/*
 * ==========================================================
 * Professional Fall Latching System
 * ==========================================================
 */

void updateFallDetection()
{
// Auto Recovery
if (fallLatched)
{
    if (!workerRecovered)
    {
        currentState = FALL_DETECTED;
        fallDetected = true;
        stateChanged = true;
        return;
    }

    // Worker সত্যিই দাঁড়িয়েছে
        fallLatched = false;
        fallDetected = false;

        freeFallDetected = false;
        impactDetected = false;
        // NOTE: lyingDetected intentionally NOT reset here.
        // detectLyingPosition() is the single owner of this flag
        // and already updates it correctly every loop.

fallStartTime = 0;
}

    // Check within timeout
    if (fallStartTime != 0 &&
    millis() - fallStartTime <= FALL_TIMEOUT)
    {
       if(!fallLatched &&
   freeFallDetected &&
   impactDetected &&
   lyingDetected)
{
    fallDetected = true;
    fallLatched = true;
    currentState = FALL_DETECTED;
    stateChanged = true;

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
    // NOTE: lyingDetected intentionally NOT reset here (see note above).

}

}

void printWorkerState()
{

    if (!stateChanged)
{
    return;
}

    Serial.println();
    Serial.println("========================================");

    switch (currentState)
    {
        case UNKNOWN_STATE:
            Serial.println("Worker State : LYING");
            break;

        case STANDING:
            Serial.println("Worker State : STANDING");
            break;

        case FORWARD_LEAN:
            Serial.println("Worker State : FORWARD LEAN");
            break;

        case BACKWARD_LEAN:
            Serial.println("Worker State : BACKWARD LEAN");
            break;

        case LEFT_LEAN:
            Serial.println("Worker State : LEFT LEAN");
            break;

        case RIGHT_LEAN:
            Serial.println("Worker State : RIGHT LEAN");
            break;

        case FALL_DETECTED:
            Serial.println("Worker State : FALL DETECTED");
            break;

        default:
            Serial.println("Worker State : UNKNOWN");
            break;
    }

    Serial.println("========================================");
}

void setup()
{
  Serial.begin(115200); 
Wire.begin(21,22); 
mpu.initialize(); 
calibrate();

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

updateWorkerState();      // আগে

updateFallDetection();    // পরে

printWorkerState();

    Serial.print("FreeFall : ");

if (freeFallDetected)
{
    Serial.println("YES");
}
else
{
    Serial.println("NO");
}

Serial.print("Impact : ");

if (impactDetected)
{
    Serial.println("YES");
}
else
{
    Serial.println("NO");
}

Serial.print("Lying : ");

if (lyingDetected)
{
    Serial.println("YES");
}
else
{
    Serial.println("NO");
}

Serial.print("Fall : ");

if (fallDetected)
{
    Serial.println("YES");
}
else
{
    Serial.println("NO");
}

Serial.print("Acceleration G: ");
Serial.println(rawAcceleration / 16384.0, 2);

    Serial.print("TiltPitch: "); 
    Serial.print(tiltPitch,2); 
    Serial.print(" TiltRoll: "); 
    Serial.println(tiltRoll,2); 

    delay(200);
}
