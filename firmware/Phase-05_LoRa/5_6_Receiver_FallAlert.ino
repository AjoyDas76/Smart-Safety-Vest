/*
  ============================================================
  PHASE 5.5 -> 5.6 - Fall Alert Display (Base Station)
  ============================================================
  Purpose: Same as 5.5 receiver, plus decodes the new
  STATUS_FALL code sent by the 5.6 transmitter when the MPU-6050
  fall detection state machine latches a confirmed fall.

  Board: ESP32 (SECOND unit - the base station)
  Wiring: identical to transmitter's LoRa wiring
    VCC->3.3V, GND->GND, SCK->18, MISO->19, MOSI->23,
    NSS->5, RESET->14, DIO0->26

  IMPORTANT: LORA_FREQUENCY must match the transmitter exactly.
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>

// ---------------- ESP32 pin mapping ----------------
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

#define LORA_FREQUENCY 433E6   // must match transmitter

// ---------------- Status codes (must match transmitter) ----------------
enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1,
  STATUS_FALL   = 2   // new in 5.6
};

// ---------------- Packet structure (must match 5.6 transmitter exactly) ----------------
struct __attribute__((packed)) DataPacket {
  uint8_t  header;       // 0xAA = marks a valid packet
  uint8_t  deviceId;
  uint16_t packetId;
  uint8_t  status;
  float    latitude;
  float    longitude;
};

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.6: Fall Alert Receiver Setup (Base Station) ==="));

  SPI.begin(18, 19, 23, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa init FAILED. Check wiring/frequency."));
    while (true) { delay(1000); }
  }

  // Must match transmitter's radio settings exactly, or packets won't decode
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println(F("Receiver configured (SF9, 125kHz, CR 4/5)."));
  Serial.println(F("Waiting for packets..."));
  Serial.println(F("deviceId,packetId,status,lat,lon,rssi,snr"));
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  if (packetSize != sizeof(DataPacket)) {
    Serial.print(F("Ignored packet - unexpected size: "));
    Serial.print(packetSize);
    Serial.print(F(" bytes (expected "));
    Serial.print(sizeof(DataPacket));
    Serial.println(F(")"));
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

  if (pkt.status == STATUS_SOS) {
    Serial.println(F("  !!! SOS ALERT - WORKER NEEDS HELP !!!"));
  }

  if (pkt.status == STATUS_FALL) {
    Serial.println(F("  !!! FALL DETECTED - WORKER MAY BE DOWN !!!"));
  }
}
