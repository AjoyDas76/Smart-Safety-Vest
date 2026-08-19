/*
  ============================================================
  PHASE 5.5 -> 5.6 - Fall Alert Transmission
  ============================================================
  5.6 adds: MPU-6050 fall detection (from Phase 3.8 finalized
  module, v2.1 logic - FreeFall + Impact + Lying confirmation +
  6s timeout) wired into the LoRa transmitter. A confirmed fall
  now sends an immediate STATUS_FALL packet, the same way the
  SOS button sends an immediate STATUS_SOS packet.

  Board: ESP32
  Wiring:
    LoRa (SX1278):
      VCC->3.3V, GND->GND, SCK->18, MISO->19, MOSI->23,
      NSS->5, RESET->14, DIO0->26
    SOS Button:
      One leg -> GPIO 4, other leg -> GND (uses internal pull-up)
    GPS (NEO-M8N):
      VCC->3.3V/5V, GND->GND, GPS TX->ESP32 GPIO16, GPS RX->ESP32 GPIO17
      (uses ESP32 hardware Serial2)
    MPU-6050 (new in 5.6):
      VCC->3.3V, GND->GND, SDA->GPIO21, SCL->GPIO22 (I2C)

  NOTE: setup() calibrates the MPU for ~7 seconds (5s countdown +
  ~2s sampling). Keep the vest still and level on the worker
  during this window, same as the standalone Phase 3.8 test.
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <MPU6050.h>
#include <math.h>

// ---------------- ESP32 pin mapping ----------------
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26
#define BUTTON_PIN 4

#define GPS_RX_PIN 16   // ESP32 pin that receives data FROM GPS TX
#define GPS_TX_PIN 17   // ESP32 pin that sends data TO GPS RX

#define I2C_SDA 21
#define I2C_SCL 22

#define LORA_FREQUENCY 433E6   // must match receiver

// Unique ID for this worker unit - change per device when you have multiple
#define DEVICE_ID 1

#define SEND_INTERVAL_MS 5000

// ---------------- Status codes ----------------
enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1,
  STATUS_FALL   = 2   // new in 5.6
};

// ---------------- Packet structure (unchanged from 5.5) ----------------
struct __attribute__((packed)) DataPacket {
  uint8_t  header;       // 0xAA = marks a valid packet
  uint8_t  deviceId;
  uint16_t packetId;     // increments each send
  uint8_t  status;        // WorkerStatus
  float    latitude;      // 0.0 if no GPS fix yet
  float    longitude;     // 0.0 if no GPS fix yet
};

TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // ESP32 Serial2

uint16_t packetCounter = 0;
unsigned long lastSend = 0;

/*
 * ==========================================================
 * MPU-6050 Fall Detection (from Phase 3.8, v2.1 - unchanged)
 * ==========================================================
 */

MPU6050 mpu;

enum WorkerMotionState {
  STANDING = 0,
  FORWARD_LEAN,
  BACKWARD_LEAN,
  LEFT_LEAN,
  RIGHT_LEAN,
  FALL_DETECTED,
  UNKNOWN_STATE
};

WorkerMotionState currentMotionState = STANDING;

int16_t ax, ay, az;
int16_t gx, gy, gz;

const int FILTER_SIZE = 10;
const int CAL_SAMPLES = 100;

long axBuf[FILTER_SIZE] = {0};
long ayBuf[FILTER_SIZE] = {0};
long azBuf[FILTER_SIZE] = {0};

int filterIndex = 0;

float axF, ayF, azF;
float pitch, roll;
float refPitch, refRoll;
float tiltPitch, tiltRoll;

float pitchThreshold = 15.0;
float rollThreshold  = 20.0;

bool motionStateChanged = false;

bool freeFallDetected = false;
float totalAcceleration = 0.0;
float rawAcceleration = 0.0;
const float FREE_FALL_THRESHOLD = 0.80;

bool impactDetected = false;
const float IMPACT_THRESHOLD = 1.8;

bool fallLatched = false;
bool workerRecovered = false;

bool lyingDetected = false;
unsigned long lyingStartTime = 0;
const unsigned long LYING_CONFIRM_TIME = 1500;
const float LYING_ANGLE = 70.0;

bool fallDetected = false;
unsigned long fallStartTime = 0;
const unsigned long FALL_TIMEOUT = 6000;

// 5.6: tracks whether we've already radioed this specific fall event
bool fallAlertSent = false;

void updateMPUFilter() {
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  axBuf[filterIndex] = ax;
  ayBuf[filterIndex] = ay;
  azBuf[filterIndex] = az;

  filterIndex = (filterIndex + 1) % FILTER_SIZE;

  long sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < FILTER_SIZE; i++) {
    sx += axBuf[i];
    sy += ayBuf[i];
    sz += azBuf[i];
  }

  axF = sx / (float)FILTER_SIZE;
  ayF = sy / (float)FILTER_SIZE;
  azF = sz / (float)FILTER_SIZE;

  pitch = atan2(azF, sqrt(axF * axF + ayF * ayF)) * 180.0 / PI;
  roll  = atan2(ayF, sqrt(axF * axF + azF * azF)) * 180.0 / PI;

  totalAcceleration = sqrt(axF * axF + ayF * ayF + azF * azF);
  rawAcceleration = sqrt((float)ax * ax + (float)ay * ay + (float)az * az);
}

void calibrateMPU() {
  Serial.println(F("MPU-6050 automatic calibration - keep the vest still"));
  for (int i = 5; i > 0; i--) {
    Serial.println(i);
    delay(1000);
  }

  float ps = 0, rs = 0;
  for (int i = 0; i < CAL_SAMPLES; i++) {
    updateMPUFilter();
    ps += pitch;
    rs += roll;
    delay(20);
  }

  refPitch = ps / CAL_SAMPLES;
  refRoll  = rs / CAL_SAMPLES;

  Serial.print(F("Reference Pitch: ")); Serial.println(refPitch, 2);
  Serial.print(F("Reference Roll: "));  Serial.println(refRoll, 2);
}

void detectFreeFall() {
  float normalizedAcceleration = rawAcceleration / 16384.0;
  if (normalizedAcceleration < FREE_FALL_THRESHOLD) {
    freeFallDetected = true;
    if (fallStartTime == 0) {
      fallStartTime = millis();
    }
  }
}

void detectImpact() {
  float normalizedAcceleration = rawAcceleration / 16384.0;
  if (normalizedAcceleration > IMPACT_THRESHOLD) {
    impactDetected = true;
  }
}

void detectLyingPosition() {
  bool horizontal = (fabs(tiltPitch) > LYING_ANGLE || fabs(tiltRoll) > LYING_ANGLE);

  if (horizontal) {
    if (lyingStartTime == 0) {
      lyingStartTime = millis();
    }
    if (millis() - lyingStartTime >= LYING_CONFIRM_TIME) {
      lyingDetected = true;
    }
  } else {
    lyingDetected = false;
    lyingStartTime = 0;
  }
}

void detectRecovery() {
  workerRecovered = (!lyingDetected && fabs(tiltPitch) < 10 && fabs(tiltRoll) < 10);
}

void updateWorkerMotionState() {
  if (fallLatched) {
    currentMotionState = FALL_DETECTED;
    return;
  }

  WorkerMotionState previousState = currentMotionState;

  if (lyingDetected) {
    currentMotionState = UNKNOWN_STATE;
    motionStateChanged = (previousState != currentMotionState);
    return;
  }

  currentMotionState = STANDING;

  if (tiltPitch < -pitchThreshold) {
    currentMotionState = FORWARD_LEAN;
  } else if (tiltPitch > pitchThreshold) {
    currentMotionState = BACKWARD_LEAN;
  } else if (tiltRoll > rollThreshold) {
    currentMotionState = LEFT_LEAN;
  } else if (tiltRoll < -rollThreshold) {
    currentMotionState = RIGHT_LEAN;
  }

  motionStateChanged = (previousState != currentMotionState);
}

void updateFallDetection() {
  // Auto recovery
  if (fallLatched) {
    if (!workerRecovered) {
      currentMotionState = FALL_DETECTED;
      fallDetected = true;
      motionStateChanged = true;
      return;
    }

    // Worker genuinely stood back up
    fallLatched = false;
    fallDetected = false;
    freeFallDetected = false;
    impactDetected = false;
    // NOTE: lyingDetected intentionally NOT reset here - detectLyingPosition()
    // is the single owner of that flag.
    fallStartTime = 0;
  }

  if (fallStartTime != 0 && millis() - fallStartTime <= FALL_TIMEOUT) {
    if (!fallLatched && freeFallDetected && impactDetected && lyingDetected) {
      fallDetected = true;
      fallLatched = true;
      currentMotionState = FALL_DETECTED;
      motionStateChanged = true;
      fallStartTime = 0;
    } else if (!fallLatched) {
      fallDetected = false;
    }
  } else {
    fallDetected = false;
    fallStartTime = 0;
    freeFallDetected = false;
    impactDetected = false;
    // NOTE: lyingDetected intentionally NOT reset here (see note above).
  }
}

/*
 * ==========================================================
 * Setup / Loop
 * ==========================================================
 */

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.6: Fall Alert Transmission ==="));

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // NEO-M8N default baud rate is 9600
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Wire.begin(I2C_SDA, I2C_SCL);
  mpu.initialize();
  calibrateMPU();

  SPI.begin(18, 19, 23, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa init FAILED. Check wiring/frequency."));
    while (true) { delay(1000); }
  }

  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20);

  Serial.println(F("Transmitter ready. Waiting for GPS fix..."));
  Serial.println(F("(GPS fix can take 30s-2min outdoors on first power-up)"));
}

void loop() {
  feedGPS();

  // ---- update fall detection state every loop ----
  updateMPUFilter();
  tiltPitch = pitch - refPitch;
  tiltRoll  = roll  - refRoll;

  detectFreeFall();
  detectImpact();
  detectLyingPosition();
  detectRecovery();
  updateWorkerMotionState();
  updateFallDetection();

  // ---- Priority 1: Manual SOS button (active LOW) ----
  if (digitalRead(BUTTON_PIN) == LOW) {
    sendPacket(STATUS_SOS);
    Serial.println(F("  >>> SOS button pressed! <<<"));
    delay(1000); // simple debounce
    return;
  }

  // ---- Priority 2: Fall alert - send once per fall event, immediately ----
  if (fallLatched && !fallAlertSent) {
    sendPacket(STATUS_FALL);
    fallAlertSent = true;
    Serial.println(F("  >>> FALL DETECTED - alert sent! <<<"));
  }

  // Reset the "already sent" flag once the worker has recovered,
  // so the next real fall can trigger a fresh alert.
  if (!fallLatched) {
    fallAlertSent = false;
  }

  // ---- Priority 3: Normal heartbeat ----
  // While a fall is still latched (worker hasn't recovered), keep
  // reporting FALL status on the heartbeat too, in case the first
  // alert packet was lost over the air.
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendPacket(fallLatched ? STATUS_FALL : STATUS_NORMAL);
  }
}

void feedGPS() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

void sendPacket(uint8_t status) {
  DataPacket pkt;
  pkt.header   = 0xAA;
  pkt.deviceId = DEVICE_ID;
  pkt.packetId = ++packetCounter;
  pkt.status   = status;

  if (gps.location.isValid()) {
    pkt.latitude  = (float)gps.location.lat();
    pkt.longitude = (float)gps.location.lng();
  } else {
    pkt.latitude  = 0.0;
    pkt.longitude = 0.0;
  }

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.print(F("Sent packet #"));
  Serial.print(pkt.packetId);
  Serial.print(F(" | status="));
  Serial.print(status == STATUS_SOS ? "SOS" : (status == STATUS_FALL ? "FALL" : "NORMAL"));
  Serial.print(F(" | GPS fix="));
  Serial.print(gps.location.isValid() ? "YES" : "NO");
  Serial.print(F(" | lat="));
  Serial.print(pkt.latitude, 6);
  Serial.print(F(" | lon="));
  Serial.println(pkt.longitude, 6);
}
