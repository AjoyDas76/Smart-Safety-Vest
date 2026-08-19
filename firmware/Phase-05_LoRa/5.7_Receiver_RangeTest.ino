/*
  ============================================================
  PHASE 5.6 -> 5.7 - Range Testing (Packet Loss + Signal Stats)
  ============================================================
  Purpose: Same as 5.6 receiver, plus tracks packet loss (using
  the packetId sequence number) and running RSSI/SNR stats, so
  a field range test can produce a clear "usable range" number
  instead of just a raw stream of packets.

  HOW TO USE FOR A RANGE TEST:
  1. Keep the transmitter fixed at one point.
  2. Walk/drive the receiver away in steps (25m, 50m, 100m...).
  3. At each distance, let ~20-30 packets arrive, then send 'r'
     over Serial (type "r" + Enter in Serial Monitor) to print
     a summary and reset the stats for the next distance.
  4. Record distance, avg RSSI, avg SNR, and loss% in a table.

  Board: ESP32 (SECOND unit - the base station)
  Wiring: identical to transmitter's LoRa wiring
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>

#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

#define LORA_FREQUENCY 433E6   // must match transmitter

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
};

// ---------------- Range test stats ----------------
uint16_t lastPacketId = 0;
bool     haveLastId = false;

unsigned long packetsReceived = 0;
unsigned long packetsMissed   = 0;

long   rssiSum = 0;
float  snrSum  = 0;
unsigned long statsCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.7: Range Testing ==="));

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
  Serial.println(F("Type 'r' + Enter any time to print a range-test summary and reset stats."));
  Serial.println(F("deviceId,packetId,status,lat,lon,rssi,snr"));
}

void loop() {
  // Allow the tester to request a summary at any time
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      printSummary();
      resetStats();
    }
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  if (packetSize != sizeof(DataPacket)) {
    Serial.print(F("Ignored packet - unexpected size: "));
    Serial.println(packetSize);
    while (LoRa.available()) LoRa.read();
    return;
  }

  DataPacket pkt;
  LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  if (pkt.header != 0xAA) {
    Serial.println(F("Dropped: bad header (corrupted packet)."));
    return;
  }

  // ---- packet loss tracking ----
  if (haveLastId) {
    uint16_t expected = lastPacketId + 1;
    if (pkt.packetId > expected) {
      packetsMissed += (pkt.packetId - expected);
    }
    // if pkt.packetId <= lastPacketId, assume a transmitter reset - don't count as loss
  }
  lastPacketId = pkt.packetId;
  haveLastId = true;
  packetsReceived++;

  // ---- signal stats ----
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

  if (pkt.status == STATUS_SOS)  Serial.println(F("  !!! SOS ALERT - WORKER NEEDS HELP !!!"));
  if (pkt.status == STATUS_FALL) Serial.println(F("  !!! FALL DETECTED - WORKER MAY BE DOWN !!!"));
}

void printSummary() {
  float lossPercent = 0;
  unsigned long totalExpected = packetsReceived + packetsMissed;
  if (totalExpected > 0) {
    lossPercent = (packetsMissed * 100.0) / totalExpected;
  }

  Serial.println();
  Serial.println(F("========== RANGE TEST SUMMARY =========="));
  Serial.print(F("Packets received : ")); Serial.println(packetsReceived);
  Serial.print(F("Packets missed   : ")); Serial.println(packetsMissed);
  Serial.print(F("Packet loss      : ")); Serial.print(lossPercent, 1); Serial.println(F(" %"));

  if (statsCount > 0) {
    Serial.print(F("Avg RSSI         : ")); Serial.print(rssiSum / (float)statsCount, 1); Serial.println(F(" dBm"));
    Serial.print(F("Avg SNR          : ")); Serial.print(snrSum / statsCount, 1); Serial.println(F(" dB"));
  }
  Serial.println(F("=========================================="));
  Serial.println(F("Record this against the current distance, then move and continue."));
  Serial.println();
}

void resetStats() {
  packetsReceived = 0;
  packetsMissed   = 0;
  rssiSum = 0;
  snrSum  = 0;
  statsCount = 0;
  haveLastId = false;
}
