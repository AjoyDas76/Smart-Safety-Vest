/*
 * Project : Smart Safety Vest
 * File    : Transmitter_Final.ino
 * Description:
 * ESP32 transmitter combining BME280 (env), MPU6050 (posture/movement/fall),
 * GPS, SOS button, battery monitor, OLED and LoRa transmission.
 *
 * Worker status reported to receiver (5 categories only):
 *   STANDING / WALKING / RUNNING / LYING / FALL
 *
 * NOTE: The finalized MPU module also detects LEAN (forward/back/left/right)
 * as an internal safety gate (it forces the movement classifier back to
 * STANDING while leaning so no stale WALKING/RUNNING carries over). Since
 * only 5 worker-status categories were requested, LEAN states are reported
 * externally as STANDING (see getSimplifiedStatus()).
 *
 * SOS RELIABILITY FIX:
 * LoRa is half-duplex — this radio physically cannot receive a "SOS_ON"/
 * "SOS_OFF" downlink from the receiver while it's in the middle of sending
 * its own 2-second telemetry packet, and vice versa. With a single fire-
 * and-forget send on the receiver's side, a downlink that happened to land
 * during that ~tens-of-ms transmit window was simply lost — which is
 * exactly the "sometimes the web SOS works, sometimes it doesn't" symptom.
 * The real fix is on the receiver (it now retries the command until
 * confirmed), but this transmitter needs to hold up its end: it now echoes
 * its current webSosActive value back in every telemetry packet (10th
 * field) so the receiver can tell whether its command actually landed.
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <LoRa.h>
#include <math.h>

// ===================== Pin Definitions =====================
#define SOS_BUTTON_PIN 15
#define BUZZER_PIN     12
#define LED_PIN        13
#define BATT_ADC_PIN   34

// LoRa Pins
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  2

// ===================== OLED Display =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===================== Sensor Objects =====================
Adafruit_BME280 bme;
MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // UART2 for GPS (RX:16, TX:17)

// ===================== Environment / GPS / System Globals =====================
float temp, hum, pres;
float latitude = 0.0, longitude = 0.0;
float batteryVoltage = 0.0;
bool sosActive = false;
bool webSosActive = false; // set by "SOS_ON"/"SOS_OFF" command from the receiver (website toggle)
unsigned long lastTxTime = 0;

/*
 * ==========================================================
 * MPU6050 Module (Posture + Movement + Fall Detection)
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

float totalAcceleration = 0.0;  // filtered accel magnitude
float rawAcceleration    = 0.0; // raw accel magnitude (free-fall/impact use)

// Posture / Lean thresholds
float pitchThreshold = 15.0;
float rollThreshold  = 20.0;

// Free Fall Detection
bool freeFallDetected = false;
const float FREE_FALL_THRESHOLD = 0.80;

// Impact Detection
bool impactDetected = false;
const float IMPACT_THRESHOLD = 1.8;

// Lying Position Detection
bool lyingDetected = false;
unsigned long lyingStartTime = 0;
const unsigned long LYING_CONFIRM_TIME = 1500; // must hold 1.5s
const float LYING_ANGLE = 70.0;

// Fall Latching
bool fallDetected = false;
bool fallLatched  = false;
bool workerRecovered = false;
unsigned long fallStartTime = 0;
const unsigned long FALL_TIMEOUT = 6000;

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
bool isUpright = true; // gates the movement classifier

// Movement-Energy Classifier (only runs while isUpright == true)
const int ENERGY_WINDOW = 15;
float energyBuf[ENERGY_WINDOW] = {0};
int energyIndex = 0;
bool energyBufFull = false;

float STANDING_ENERGY_MAX = 1500.0;
float WALKING_ENERGY_MAX  = 6000.0;
// anything above WALKING_ENERGY_MAX (while upright) = RUNNING

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

  const char* status; // full status (includes LEAN, for local debug/OLED)
};

MPUData mpuData;

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

  if (lyingDetected)
  {
    postureState = POSTURE_LYING;
    isUpright = false;
    postureChanged = (previousPostureState != postureState);
    previousPostureState = postureState;
    return;
  }

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
  }
}

// Full status (for local OLED/Serial debugging - includes LEAN detail)
const char* getFullWorkerStatus()
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

// Simplified status - ONLY the 5 requested categories, sent over LoRa
// LEAN states collapse into STANDING here (movement classifier already
// forces STANDING while leaning, so this stays consistent).
const char* getSimplifiedStatus()
{
  switch (postureState)
  {
    case POSTURE_FALL: return "FALL";
    case POSTURE_LYING: return "LYING";
    default:
      switch (movementState)
      {
        case MOVEMENT_WALKING: return "WALKING";
        case MOVEMENT_RUNNING: return "RUNNING";
        default:                return "STANDING";
      }
  }
}

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

  mpuData.status = getFullWorkerStatus();
}

void calibrateMPU()
{
  Serial.println("[MPU] Automatic Calibration - keep the worker still...");
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

  Serial.print("[MPU] Reference Pitch: "); Serial.println(refPitch, 2);
  Serial.print("[MPU] Reference Roll: ");  Serial.println(refRoll, 2);

  // Warm up the energy buffer while standing still
  for (int i = 0; i < ENERGY_WINDOW * 2; i++)
  {
    updateMPUFilter();
    delay(20);
  }
}

/*
 * ==========================================================
 * Setup
 * ==========================================================
 */
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[System] Initializing Smart Vest Transmitter...");

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  Wire.begin(); // I2C for OLED + BME280 + MPU6050 (default SDA21/SCL22)

  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OLED] Failed to initialize!");
  } else {
    Serial.println("[OLED] Ready.");
  }

  // Initialize BME280
  if (!bme.begin(0x76)) Serial.println("[BME280] Sensor Failed!");
  else Serial.println("[BME280] Sensor Ready.");

  // Initialize MPU6050 + calibrate (worker must stand still during countdown)
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("[MPU6050] Sensor Ready.");
    calibrateMPU();
  } else {
    Serial.println("[MPU6050] Connection Failed!");
  }

  // Initialize LoRa
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("[LoRa] Transmitter Initialization Failed!");
    while (1);
  }
  Serial.println("[LoRa] Transmitter Ready.\n");
}

/*
 * ==========================================================
 * Loop
 * ==========================================================
 */
void loop() {
  // 0. Check for incoming LoRa downlink command (Web SOS toggle relayed by the receiver)
  int cmdSize = LoRa.parsePacket();
  if (cmdSize) {
    String cmd = "";
    while (LoRa.available()) cmd += (char)LoRa.read();
    cmd.trim();
    if (cmd == "SOS_ON") {
      webSosActive = true;
      Serial.println("[LoRa] Web SOS command received: ON");
    } else if (cmd == "SOS_OFF") {
      webSosActive = false;
      Serial.println("[LoRa] Web SOS command received: OFF");
    }
  }

  // 1. Read BME280
  temp = bme.readTemperature();
  hum = bme.readHumidity();
  pres = bme.readPressure() / 100.0F;

  // 2. Update MPU6050 posture / movement / fall pipeline
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

  // 3. Read GPS Data
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
  }

  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  // 4. Read SOS & Battery
  sosActive = (digitalRead(SOS_BUTTON_PIN) == LOW);
  batteryVoltage = (analogRead(BATT_ADC_PIN) / 4095.0) * 2 * 3.3 * 1.1;

  // 5. Trigger Alarm
  if (mpuData.fall || sosActive || webSosActive || temp > 50.0) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  // 6. Update OLED Display (shows full status incl. LEAN, useful on-body)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Temp: %.1f C  Hum: %.1f%%\n", temp, hum);
  display.printf("Pres: %.1f hPa\n", pres);
  display.printf("State: %s\n", getFullWorkerStatus());
  if (mpuData.fall) display.println("ALERT: FALL DETECTED!");
  if (sosActive) display.println("ALERT: SOS PRESSED!");
  if (webSosActive) display.println("ALERT: WEB SOS ACTIVE!");
  display.display();

  // 7. LoRa Transmission & Serial Print (Every 2 Seconds)
  if (millis() - lastTxTime > 2000) {
    const char* statusToSend = getSimplifiedStatus(); // STANDING/WALKING/RUNNING/LYING/FALL

    // NOTE: webSosActive is appended as a 10th field so the receiver can
    // confirm that a "SOS_ON"/"SOS_OFF" downlink command it sent actually
    // reached and was applied by this transmitter (see the receiver's
    // confirmedWebSos/retry logic — this is the "ack" for that retry loop).
    String payload = String(temp, 1) + "," + String(hum, 1) + "," + String(pres, 1) + "," +
                     String(statusToSend) + "," + String(mpuData.fall) + "," +
                     String(latitude, 6) + "," + String(longitude, 6) + "," +
                     String(sosActive) + "," + String(batteryVoltage, 2) + "," +
                     String(webSosActive);

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    // Print Data to Serial Monitor
    Serial.println("------ [ TRANSMITTING SENSOR DATA ] ------");
    Serial.printf("Temp: %.1f C | Hum: %.1f %% | Pres: %.1f hPa\n", temp, hum, pres);
    Serial.printf("Status (sent): %s | Status (full): %s\n", statusToSend, getFullWorkerStatus());
    Serial.printf("Fall: %s | SOS(button): %s | SOS(web): %s\n", mpuData.fall ? "YES" : "NO", sosActive ? "YES" : "NO", webSosActive ? "YES" : "NO");
    Serial.printf("GPS: Lat %.6f, Lng %.6f | Batt: %.2fV\n", latitude, longitude, batteryVoltage);
    Serial.printf("Payload Sent: %s\n", payload.c_str());
    Serial.println("------------------------------------------\n");

    lastTxTime = millis();
  }

  delay(20); // keep MPU sample rate steady for the gyro energy window
}
