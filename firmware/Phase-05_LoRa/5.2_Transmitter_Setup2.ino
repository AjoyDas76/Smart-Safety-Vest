/*
  ============================================================
  PHASE 5.2 - LoRa Transmitter Setup (Safety Vest / Worker Unit)
  ============================================================
  Purpose: Properly configure the transmitter's radio settings
  (spreading factor, bandwidth, coding rate, TX power) and send
  a basic structured packet on a fixed interval.

  This builds the FOUNDATION that 5.4 (worker status), 5.5 (GPS),
  and 5.6 (fall alert) will plug data into later. For now the
  packet just carries a device ID + counter + placeholder status,
  so we can confirm structured sending is working correctly.

  Board: ESP32
  Wiring:
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

#define SEND_INTERVAL_MS 5000

// ---------------- Packet structure (foundation for later phases) ----------------
// Keeping it as a simple struct now - GPS/fall/status fields get added in 5.4-5.6
struct __attribute__((packed)) DataPacket {
  uint8_t  header;       // 0xAA = marks a valid packet
  uint8_t  deviceId;
  uint16_t packetId;     // increments each send
  uint8_t  status;        // placeholder for now (0 = normal), used properly in 5.4
};

uint16_t packetCounter = 0;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.2: LoRa Transmitter Setup ==="));

  SPI.begin(18, 19, 23, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa init FAILED. Check wiring/frequency."));
    while (true) { delay(1000); }
  }

  // ---- Radio configuration ----
  // Spreading Factor: 7 (fast, shorter range) to 12 (slow, longer range)
  // Start at 9 as a balanced default - we'll tune this properly in 5.7 (Range Testing)
  LoRa.setSpreadingFactor(9);

  // Bandwidth: wider = faster but more susceptible to noise
  LoRa.setSignalBandwidth(125E3);

  // Coding rate: higher = more error correction overhead, more robust
  LoRa.setCodingRate4(5);

  // TX power: RA-02 supports up to 20 dBm (max range)
  LoRa.setTxPower(20);

  Serial.println(F("Transmitter configured:"));
  Serial.println(F("  Spreading Factor: 9"));
  Serial.println(F("  Bandwidth: 125 kHz"));
  Serial.println(F("  Coding Rate: 4/5"));
  Serial.println(F("  TX Power: 20 dBm"));
  Serial.println(F("Starting transmission loop..."));
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendPacket();
  }
}

void sendPacket() {
  DataPacket pkt;
  pkt.header   = 0xAA;
  pkt.deviceId = DEVICE_ID;
  pkt.packetId = ++packetCounter;
  pkt.status   = 0; // placeholder - real status logic comes in 5.4

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.print(F("Sent packet #"));
  Serial.print(pkt.packetId);
  Serial.print(F(" | deviceId="));
  Serial.print(pkt.deviceId);
  Serial.print(F(" | size="));
  Serial.print(sizeof(pkt));
  Serial.println(F(" bytes"));
}
