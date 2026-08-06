/*
  ============================================================
  PHASE 5.2 -> 5.4 - LoRa Transmitter Setup + Worker Status Transmission
  ============================================================
  5.4 adds: a manual SOS button and real status transmission.
  Status codes:
    0 = NORMAL   (heartbeat, sent every SEND_INTERVAL_MS)
    1 = SOS      (worker pressed the button - sent immediately)

  Board: ESP32
  Wiring:
    VCC->3.3V, GND->GND, SCK->18, MISO->19, MOSI->23,
    NSS->5, RESET->14, DIO0->26
  Button (new in 5.4):
    One leg -> GPIO 4, other leg -> GND (uses internal pull-up)
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>

// ---------------- ESP32 pin mapping ----------------
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26
#define BUTTON_PIN 4

#define LORA_FREQUENCY 433E6   // must match receiver

// Unique ID for this worker unit - change per device when you have multiple
#define DEVICE_ID 1

#define SEND_INTERVAL_MS 5000

// ---------------- Status codes (5.4) ----------------
enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1
};

// ---------------- Packet structure ----------------
struct __attribute__((packed)) DataPacket {
  uint8_t  header;       // 0xAA = marks a valid packet
  uint8_t  deviceId;
  uint16_t packetId;     // increments each send
  uint8_t  status;        // WorkerStatus
};

uint16_t packetCounter = 0;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.4: Worker Status Transmission ==="));

  pinMode(BUTTON_PIN, INPUT_PULLUP);

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

  Serial.println(F("Transmitter ready. Button on GPIO4 sends SOS."));
}

void loop() {
  // Manual SOS button (active LOW) - send immediately, no waiting for interval
  if (digitalRead(BUTTON_PIN) == LOW) {
    sendPacket(STATUS_SOS);
    Serial.println(F("  >>> SOS button pressed! <<<"));
    delay(1000); // simple debounce / prevents spamming while held down
    return;
  }

  // Normal heartbeat status
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendPacket(STATUS_NORMAL);
  }
}

void sendPacket(uint8_t status) {
  DataPacket pkt;
  pkt.header   = 0xAA;
  pkt.deviceId = DEVICE_ID;
  pkt.packetId = ++packetCounter;
  pkt.status   = status;

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.print(F("Sent packet #"));
  Serial.print(pkt.packetId);
  Serial.print(F(" | status="));
  Serial.println(status == STATUS_SOS ? "SOS" : "NORMAL");
}
