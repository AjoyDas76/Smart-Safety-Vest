/*
 * Project : Smart Safety Vest
 * Firmware: Smart_Safety_Vest.ino
 * Version : v2.1
 * Module  : Motion State Machine + Fall Detection (Timing Fix)
 * Author  : Ajoy Das Team
 *
 * v2.1 Changelog (root cause of MISSED falls found - timing, not threshold):
 * - Reported issue: real falls still not triggering FALL DETECTED
 *   even after v2.0 threshold correction.
 * - Root cause found by tracing the serial log: in more than one
 *   real-fall attempt, FreeFall+Impact triggered correctly, but the
 *   worker's body took 2.5-3s of stumbling/rolling before settling
 *   into a stable lying position. TiltPitch crossed the 70-degree
 *   LYING_ANGLE right around the same moment FALL_TIMEOUT (3000ms)
 *   expired and reset all flags - so the required extra 1500ms
 *   (LYING_CONFIRM_TIME) to confirm lying never had a chance to run.
 *   A real fall is rarely instantaneous; the old 3000ms budget left
 *   almost no slack once the 1500ms lying-confirmation cost is
 *   subtracted.
 * - FALL_TIMEOUT: 3000 -> 6000ms. This gives roughly 4.5s for the
 *   free-fall-to-settled-lying sequence to complete, plus the 1.5s
 *   confirmation, before the window closes.
 *
 * v2.0 Changelog (CORRECTION - v1.8 tuning was too aggressive):
 * - Reported issue: real falls were being MISSED (no FALL DETECTED).
 * - Root cause: v1.8 tightened FREE_FALL_THRESHOLD to 0.50 and
 *   IMPACT_THRESHOLD to 2.2 based on normal-movement noise concerns.
 *   But re-checking the actual confirmed real-fall test log showed
 *   genuine falls with a free-fall dip as high as 0.65g and an
 *   impact spike as low as 1.96g - both inside the "safe" zone of
 *   the v1.8 thresholds, so those falls would no longer trigger.
 * - FREE_FALL_THRESHOLD: 0.50 -> 0.80 (above the highest confirmed
 *   real dip of 0.65g, with margin)
 * - IMPACT_THRESHOLD: 2.2 -> 1.8 (below the lowest confirmed real
 *   impact of 1.96g, with margin)
 * - Why this is still safe: the bending/kneeling stress test log
 *   showed tiltPitch reaching as far as -88 degrees during normal
 *   fast movement, but LYING never confirmed because it never held
 *   still for the full 1.5s. That confirms false-alarm protection
 *   comes from the Lying confirmation timer, not from tight
 *   FreeFall/Impact thresholds - so loosening these two is low risk.
 *
 * v1.9 Changelog:
 * - DIAGNOSTIC: Uncommented TiltPitch/TiltRoll serial print.
 *   Reported issue: Lying never triggers from BACKWARD_LEAN, only
 *   from FORWARD_LEAN. Root cause can't be confirmed without seeing
 *   the actual tilt angle (in degrees) reached during a backward
 *   lean/lie-down test, since LYING_ANGLE = 70.0 depends on tiltPitch
 *   or tiltRoll actually crossing that value. This print exposes
 *   those numbers so we can see how close backward motion gets to 70.
 *
 * v1.7 Changelog:
 * - FIX: updateFallDetection() no longer force-resets lyingDetected.
 *        Previously it wiped lyingDetected back to false every loop
 *        whenever no fall sequence was active (fallStartTime == 0),
 *        right after detectLyingPosition() had set it correctly.
 *        This corrupted the "Lying" debug flag and could interfere
 *        with future logic that reads lyingDetected directly.
 *        detectLyingPosition() is now the single owner of this flag.
 * - FIX: Comment corrected (1 second -> 1.5 seconds) to match
 *        LYING_CONFIRM_TIME = 1500.
 *
 * v1.8 Changelog (threshold tuning based on real serial log data):
 * - FREE_FALL_THRESHOLD: 0.70 -> 0.50
 *   Real captured falls showed dips as low as 0.22-0.65g, but so did
 *   ordinary quick lean/turn movements. 0.50 reduces how often normal
 *   work motion is flagged as free-fall, while still catching every
 *   confirmed fall dip in the test log that went below ~0.5g.
 * - IMPACT_THRESHOLD: 1.7 -> 2.2
 *   Real fall impacts in the log landed mostly at 1.9g-3.5g. Ordinary
 *   lean movements were also seen spiking past 1.7g (sometimes even
 *   past 2.2g), so this alone will not fully separate the two -
 *   the Lying confirmation timer remains the main safety net and
 *   worked perfectly in testing (zero false FALL DETECTED events).
 *   Do not rely on FreeFall/Impact thresholds alone; keep validating
 *   with real bending/kneeling tests (see note below).
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