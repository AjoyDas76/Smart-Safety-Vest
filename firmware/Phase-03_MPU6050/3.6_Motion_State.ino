/*
 * Project : Smart Safety Vest
 * Firmware: Smart_Safety_Vest.ino
 * Version : v1.6
 * Module  : Motion State Machine
 * Author  : Ajoy Das Team
 */

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

float pitchThreshold = 20.0;
float rollThreshold  = 20.0;

/*
 * ==========================================================
 * Motion Detection Flag
 * ==========================================================
 */

bool stateChanged = false;

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

void updateWorkerState()
{
    WorkerState previousState = currentState;

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


    updateWorkerState();
    printWorkerState();


    delay(200);
}