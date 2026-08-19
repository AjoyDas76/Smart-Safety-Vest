/*
  ============================================================
  PHASE 5.7 - Range Testing (Transmitter / Worker Unit Side)
  ============================================================
  Purpose: Counterpart to 5_7_Receiver_RangeTest.ino. Sends
  numbered packets at a FAST, FIXED interval (not the normal
  5s heartbeat) so a field range test can gather ~20-30 packets
  per distance point quickly.

  This transmitter is intentionally standalone/simple - no MPU,
  no real GPS reading - so it can be flashed onto a spare ESP32
  and carried around during the walk/drive-away range test,
  without needing the full vest hardware assembled.

  HOW TO USE FOR A RANGE TEST:
  1. Power this up at a fixed point (or have someone hold it
     stationary) - this is your reference transmitter.
  2. Walk/drive the RECEIVER away in steps (25m, 50m, 100m...).
  3. At each distance, let the receiver log ~20-30 packets
     (default: 1 packet/second -> ~20-30 seconds), then send
     'r' to the receiver's Serial Monitor to print a summary.
  4. Record distance, avg RSSI, avg SNR, and loss% in a table.

  Board: ESP32
  Wiring: identical to other Phase 5 transmitters
    VCC->3.3V, GND->GND, SCK->18, MISO->19, MOSI->23,
    NSS->5, RESET->14, DIO0->26
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>

// ---------------- ESP32 pin mapping ----------------
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

#define LORA_FREQUENCY 433E6   // must match receiver

// Unique ID for this worker unit - change per device when you have multiple
#define DEVICE_ID 1

// Fast interval for range testing (NOT the normal 5000ms heartbeat).
// At 1000ms, ~20-30 packets arrive in 20-30 seconds per distance point.
#define SEND_INTERVAL_MS 1000

// ---------------- Status codes (must match 5.7 receiver) ----------------
enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1,
  STATUS_FALL   = 2
};

// ---------------- Packet structure (must match 5_7_Receiver_RangeTest.ino exactly) ----------------
struct __attribute__((packed)) DataPacket {
  uint8_t  header;
  uint8_t  deviceId;
  uint16_t packetId;
  uint8_t  status;
  float    latitude;
  float    longitude;
};

uint16_t packetCounter = 0;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.7: Range Testing (Transmitter) ==="));

  SPI.begin(18, 19, 23, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa init FAILED. Check wiring/frequency."));
    while (true) { delay(1000); }
  }

  // Must match receiver's radio settings exactly
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20);

  Serial.println(F("Transmitter configured (SF9, 125kHz, CR 4/5, 20dBm)."));
  Serial.print(F("Sending a packet every "));
  Serial.print(SEND_INTERVAL_MS);
  Serial.println(F(" ms for range testing..."));
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendPacket();
  }
}

void sendPacket() {
  DataPacket pkt;
  pkt.header    = 0xAA;
  pkt.deviceId  = DEVICE_ID;
  pkt.packetId  = ++packetCounter;
  pkt.status    = STATUS_NORMAL;

  // No real GPS on this standalone range-test unit - placeholder coordinates.
  // (Swap these lines out for real gps.location.lat()/lng() if a GPS module
  // is attached during the test.)
  pkt.latitude  = 0.0;
  pkt.longitude = 0.0;

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.print(F("Sent packet #"));
  Serial.println(pkt.packetId);
}
