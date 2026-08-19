/*
  ============================================================
  PHASE 5.9 - LoRa Communication Module Finalization (RECEIVER)
  ============================================================
  FIRMWARE_VERSION: LoRa Module v1.0 (Final)

  This is the finalized base-station LoRa receiver for the
  Smart Safety Vest, closing out Phase 5. Counterpart to
  5.9_Transmitter_Final.ino. Combines:

    - 5.3      Core LoRa receiver setup
    - 5.4      Worker status decoding (NORMAL / SOS)
    - 5.5      GPS coordinate decoding
    - 5.6      Fall alert decoding (STATUS_FALL)
    - 5.7      Range-test style stats (packet loss / RSSI / SNR)
               kept in, since they're useful for ongoing field
               monitoring, not just one-off range tests
    - 5.8      Checksum validation + ACK reply for critical alerts

  This is the receiver Phase 9 (System Integration) should build
  on - e.g. by forwarding decoded packets to Firebase (Phase 6).

  Board: ESP32 (SECOND unit - the base station)
  Wiring: identical to transmitter's LoRa wiring
    VCC->3.3V, GND->GND, SCK->18, MISO->19, MOSI->23,
    NSS->5, RESET->14, DIO0->26

  IMPORTANT: LORA_FREQUENCY and radio settings must match the
  transmitter exactly.
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>

#define FIRMWARE_VERSION "LoRa Module v1.0 (Final)"

#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

#define LORA_FREQUENCY 433E6

#define DATA_HEADER 0xAA
#define ACK_HEADER  0xAC

enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1,
  STATUS_FALL   = 2
};

struct __attribute__((packed)) DataPacket {
  uint8_t  header;
  uint8_t  deviceId;
  uint16_t packetId;
  uint8_t  status;
  float    latitude;
  float    longitude;
  uint8_t  checksum;
};

struct __attribute__((packed)) AckPacket {
  uint8_t  header;
  uint8_t  deviceId;
  uint16_t packetId;
  uint8_t  checksum;
};

// ---------------- Ongoing field stats (from 5.7/5.8) ----------------
uint16_t lastPacketId = 0;
bool     haveLastId = false;

unsigned long packetsReceived = 0;
unsigned long packetsMissed   = 0;
unsigned long packetsCorrupted = 0;

long   rssiSum = 0;
float  snrSum  = 0;
unsigned long statsCount = 0;

uint8_t computeChecksum(const uint8_t* data, size_t len) {
  uint8_t chk = 0;
  for (size_t i = 0; i < len; i++) chk ^= data[i];
  return chk;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.print(F("=== Smart Safety Vest - "));
  Serial.print(FIRMWARE_VERSION);
  Serial.println(F(" (Receiver / Base Station) ==="));

  SPI.begin(18, 19, 23, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa init FAILED. Check wiring/frequency."));
    while (true) { delay(1000); }
  }

  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println(F("Receiver configured (SF9, 125kHz, CR 4/5)."));
  Serial.println(F("Type 'r' + Enter any time for a stats summary."));
  Serial.println(F("deviceId,packetId,status,lat,lon,rssi,snr"));
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') { printSummary(); resetStats(); }
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  if (packetSize != sizeof(DataPacket)) {
    // Not a data packet (e.g. stray ACK-sized packet, or noise) - ignore quietly
    while (LoRa.available()) LoRa.read();
    return;
  }

  DataPacket pkt;
  LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  if (pkt.header != DATA_HEADER) {
    Serial.println(F("Dropped: bad header."));
    return;
  }

  // ---- checksum validation ----
  uint8_t expectedChk = computeChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);
  if (pkt.checksum != expectedChk) {
    packetsCorrupted++;
    Serial.print(F("Dropped: checksum mismatch (packet #"));
    Serial.print(pkt.packetId);
    Serial.println(F(" corrupted in transit)."));
    return;
  }

  // ---- packet loss tracking ----
  if (haveLastId) {
    uint16_t expected = lastPacketId + 1;
    if (pkt.packetId > expected) {
      packetsMissed += (pkt.packetId - expected);
    }
  }
  lastPacketId = pkt.packetId;
  haveLastId = true;
  packetsReceived++;

  rssiSum += rssi;
  snrSum  += snr;
  statsCount++;

  String statusText = "NORMAL";
  if (pkt.status == STATUS_SOS)  statusText = "SOS";
  if (pkt.status == STATUS_FALL) statusText = "FALL";

  Serial.print(pkt.deviceId);   Serial.print(',');
  Serial.print(pkt.packetId);   Serial.print(',');
  Serial.print(statusText);     Serial.print(',');
  Serial.print(pkt.latitude, 6);  Serial.print(',');
  Serial.print(pkt.longitude, 6); Serial.print(',');
  Serial.print(rssi);           Serial.print(',');
  Serial.println(snr);

  if (pkt.latitude == 0.0 && pkt.longitude == 0.0) {
    Serial.println(F("  (note: worker unit has no GPS fix yet)"));
  }

  if (pkt.status == STATUS_SOS)  Serial.println(F("  !!! SOS ALERT - WORKER NEEDS HELP !!!"));
  if (pkt.status == STATUS_FALL) Serial.println(F("  !!! FALL DETECTED - WORKER MAY BE DOWN !!!"));

  // ---- ACK back for critical alerts ----
  if (pkt.status == STATUS_SOS || pkt.status == STATUS_FALL) {
    sendAck(pkt.deviceId, pkt.packetId);
  }
}

void sendAck(uint8_t deviceId, uint16_t packetId) {
  AckPacket ack;
  ack.header   = ACK_HEADER;
  ack.deviceId = deviceId;
  ack.packetId = packetId;
  ack.checksum = computeChecksum((uint8_t*)&ack, sizeof(ack) - 1);

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&ack, sizeof(ack));
  LoRa.endPacket();

  LoRa.receive();  // resume listening - beginPacket/endPacket suspends receive mode

  Serial.print(F("  -> ACK sent for packet #"));
  Serial.println(packetId);
}

void printSummary() {
  float lossPercent = 0;
  unsigned long totalExpected = packetsReceived + packetsMissed;
  if (totalExpected > 0) lossPercent = (packetsMissed * 100.0) / totalExpected;

  Serial.println();
  Serial.println(F("========== LORA LINK SUMMARY =========="));
  Serial.print(F("Packets received : ")); Serial.println(packetsReceived);
  Serial.print(F("Packets missed   : ")); Serial.println(packetsMissed);
  Serial.print(F("Packets corrupted: ")); Serial.println(packetsCorrupted);
  Serial.print(F("Packet loss      : ")); Serial.print(lossPercent, 1); Serial.println(F(" %"));

  if (statsCount > 0) {
    Serial.print(F("Avg RSSI         : ")); Serial.print(rssiSum / (float)statsCount, 1); Serial.println(F(" dBm"));
    Serial.print(F("Avg SNR          : ")); Serial.print(snrSum / statsCount, 1); Serial.println(F(" dB"));
  }
  Serial.println(F("=========================================="));
  Serial.println();
}

void resetStats() {
  packetsReceived = 0;
  packetsMissed   = 0;
  packetsCorrupted = 0;
  rssiSum = 0;
  snrSum  = 0;
  statsCount = 0;
  haveLastId = false;
}
