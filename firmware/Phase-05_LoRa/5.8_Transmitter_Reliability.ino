/*
  ============================================================
  PHASE 5.8 - Packet Reliability & Error Handling (Transmitter)
  ============================================================
  Adds on top of 5.6:
    1. A checksum byte on every packet, so the receiver can
       detect and drop corrupted packets instead of trusting
       garbled data.
    2. An ACK + retry mechanism for SOS and FALL packets only
       (routine NORMAL heartbeats stay fire-and-forget - they
       repeat every 5s anyway, so losing one occasionally is
       fine, but an emergency alert must not be).

  If no ACK comes back after MAX_RETRIES attempts, the packet
  is still considered "sent" locally (buzzer already fired),
  but Serial logs a warning so you know the base station may
  not have gotten it - useful during range testing too.

  Wiring: unchanged from 5.6.
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

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

#define I2C_SDA 21
#define I2C_SCL 22

#define LORA_FREQUENCY 433E6

#define DEVICE_ID 1
#define SEND_INTERVAL_MS 5000

// ---------------- 5.8: reliability settings ----------------
#define DATA_HEADER 0xAA
#define ACK_HEADER  0xAC
#define MAX_RETRIES 3
#define ACK_TIMEOUT_MS 1200

// ---------------- Status codes ----------------
enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1,
  STATUS_FALL   = 2
};

// ---------------- Packet structures ----------------
struct __attribute__((packed)) DataPacket {
  uint8_t  header;
  uint8_t  deviceId;
  uint16_t packetId;
  uint8_t  status;
  float    latitude;
  float    longitude;
  uint8_t  checksum;   // new in 5.8 - XOR over every byte above
};

struct __attribute__((packed)) AckPacket {
  uint8_t  header;     // ACK_HEADER
  uint8_t  deviceId;   // echoes the device being acknowledged
  uint16_t packetId;   // echoes the packetId being acknowledged
  uint8_t  checksum;
};

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

uint16_t packetCounter = 0;
unsigned long lastSend = 0;

/* ========================================================
   MPU-6050 Fall Detection (from Phase 3.8 / 5.6, unchanged)
   ======================================================== */

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

bool fallAlertSent = false;

void updateMPUFilter() {
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
}

void calibrateMPU() {
  Serial.println(F("MPU-6050 automatic calibration - keep the vest still"));
  for (int i = 5; i > 0; i--) { Serial.println(i); delay(1000); }

  float ps = 0, rs = 0;
  for (int i = 0; i < CAL_SAMPLES; i++) { updateMPUFilter(); ps += pitch; rs += roll; delay(20); }

  refPitch = ps / CAL_SAMPLES;
  refRoll  = rs / CAL_SAMPLES;
  Serial.print(F("Reference Pitch: ")); Serial.println(refPitch, 2);
  Serial.print(F("Reference Roll: "));  Serial.println(refRoll, 2);
}

void detectFreeFall() {
  float n = rawAcceleration / 16384.0;
  if (n < FREE_FALL_THRESHOLD) {
    freeFallDetected = true;
    if (fallStartTime == 0) fallStartTime = millis();
  }
}

void detectImpact() {
  float n = rawAcceleration / 16384.0;
  if (n > IMPACT_THRESHOLD) impactDetected = true;
}

void detectLyingPosition() {
  bool horizontal = (fabs(tiltPitch) > LYING_ANGLE || fabs(tiltRoll) > LYING_ANGLE);
  if (horizontal) {
    if (lyingStartTime == 0) lyingStartTime = millis();
    if (millis() - lyingStartTime >= LYING_CONFIRM_TIME) lyingDetected = true;
  } else {
    lyingDetected = false;
    lyingStartTime = 0;
  }
}

void detectRecovery() {
  workerRecovered = (!lyingDetected && fabs(tiltPitch) < 10 && fabs(tiltRoll) < 10);
}

void updateWorkerMotionState() {
  if (fallLatched) { currentMotionState = FALL_DETECTED; return; }

  WorkerMotionState previousState = currentMotionState;

  if (lyingDetected) {
    currentMotionState = UNKNOWN_STATE;
    motionStateChanged = (previousState != currentMotionState);
    return;
  }

  currentMotionState = STANDING;
  if (tiltPitch < -pitchThreshold) currentMotionState = FORWARD_LEAN;
  else if (tiltPitch > pitchThreshold) currentMotionState = BACKWARD_LEAN;
  else if (tiltRoll > rollThreshold) currentMotionState = LEFT_LEAN;
  else if (tiltRoll < -rollThreshold) currentMotionState = RIGHT_LEAN;

  motionStateChanged = (previousState != currentMotionState);
}

void updateFallDetection() {
  if (fallLatched) {
    if (!workerRecovered) {
      currentMotionState = FALL_DETECTED;
      fallDetected = true;
      motionStateChanged = true;
      return;
    }
    fallLatched = false;
    fallDetected = false;
    freeFallDetected = false;
    impactDetected = false;
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
  }
}

/* ========================================================
   5.8: Checksum + ACK/retry reliability layer
   ======================================================== */

uint8_t computeChecksum(const uint8_t* data, size_t len) {
  uint8_t chk = 0;
  for (size_t i = 0; i < len; i++) chk ^= data[i];
  return chk;
}

// Listens for an AckPacket matching (deviceId, packetId) within timeoutMs.
bool waitForAck(uint8_t deviceId, uint16_t packetId, unsigned long timeoutMs) {
  unsigned long start = millis();
  LoRa.receive();

  while (millis() - start < timeoutMs) {
    int sz = LoRa.parsePacket();
    if (sz == sizeof(AckPacket)) {
      AckPacket ack;
      LoRa.readBytes((uint8_t*)&ack, sizeof(ack));
      uint8_t expectedChk = computeChecksum((uint8_t*)&ack, sizeof(ack) - 1);

      if (ack.header == ACK_HEADER && ack.checksum == expectedChk &&
          ack.deviceId == deviceId && ack.packetId == packetId) {
        return true;
      }
      // else: not our ACK (corrupted or stale) - keep listening until timeout
    }
  }
  return false;
}

void transmitPacket(const DataPacket &pkt) {
  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();
}

void sendPacket(uint8_t status) {
  DataPacket pkt;
  pkt.header   = DATA_HEADER;
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

  pkt.checksum = computeChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

  bool critical = (status == STATUS_SOS || status == STATUS_FALL);
  int maxAttempts = critical ? MAX_RETRIES : 1;
  bool acked = false;

  for (int attempt = 1; attempt <= maxAttempts && !acked; attempt++) {
    transmitPacket(pkt);
    logSend(pkt, attempt);

    if (critical) {
      acked = waitForAck(pkt.deviceId, pkt.packetId, ACK_TIMEOUT_MS);
      if (acked) {
        Serial.println(F("  ACK received - base station confirmed receipt."));
      } else if (attempt < maxAttempts) {
        Serial.print(F("  No ACK - retrying (attempt "));
        Serial.print(attempt + 1);
        Serial.print(F("/"));
        Serial.print(maxAttempts);
        Serial.println(F(")..."));
      } else {
        Serial.println(F("  WARNING: No ACK after max retries. Base station may not have received this alert."));
      }
    }
  }
}

void logSend(const DataPacket &pkt, int attempt) {
  Serial.print(F("Sent packet #"));
  Serial.print(pkt.packetId);
  if (attempt > 1) { Serial.print(F(" (retry ")); Serial.print(attempt); Serial.print(F(")")); }
  Serial.print(F(" | status="));
  Serial.print(pkt.status == STATUS_SOS ? "SOS" : (pkt.status == STATUS_FALL ? "FALL" : "NORMAL"));
  Serial.print(F(" | GPS fix="));
  Serial.print(gps.location.isValid() ? "YES" : "NO");
  Serial.print(F(" | lat="));
  Serial.print(pkt.latitude, 6);
  Serial.print(F(" | lon="));
  Serial.println(pkt.longitude, 6);
}

/* ========================================================
   Setup / Loop
   ======================================================== */

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.8: Packet Reliability & Error Handling ==="));

  pinMode(BUTTON_PIN, INPUT_PULLUP);
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
}

void loop() {
  feedGPS();

  updateMPUFilter();
  tiltPitch = pitch - refPitch;
  tiltRoll  = roll  - refRoll;

  detectFreeFall();
  detectImpact();
  detectLyingPosition();
  detectRecovery();
  updateWorkerMotionState();
  updateFallDetection();

  // Priority 1: SOS button
  if (digitalRead(BUTTON_PIN) == LOW) {
    sendPacket(STATUS_SOS);
    Serial.println(F("  >>> SOS button pressed! <<<"));
    delay(1000);
    return;
  }

  // Priority 2: Fall alert (edge-triggered, once per fall event)
  if (fallLatched && !fallAlertSent) {
    sendPacket(STATUS_FALL);
    fallAlertSent = true;
    Serial.println(F("  >>> FALL DETECTED - alert sent! <<<"));
  }

  if (!fallLatched) {
    fallAlertSent = false;
  }

  // Priority 3: routine heartbeat (no ACK requested)
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
