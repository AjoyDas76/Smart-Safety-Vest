/*
  ============================================================
  PHASE 5.4 -> 5.5 - Worker Status + GPS Data Transmission
  ============================================================
  5.5 adds: NEO-M8N GPS module, latitude/longitude added to
  every transmitted packet.

  Board: ESP32
  Wiring:
    VCC->3.3V, GND->GND, SCK->18, MISO->19, MOSI->23,
    NSS->5, RESET->14, DIO0->26
  Button:
    One leg -> GPIO 4, other leg -> GND (uses internal pull-up)
  GPS (NEO-M8N, new in 5.5):
    VCC->3.3V/5V, GND->GND, GPS TX->ESP32 GPIO16, GPS RX->ESP32 GPIO17
    (uses ESP32 hardware Serial2)
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

// ---------------- ESP32 pin mapping ----------------
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26
#define BUTTON_PIN 4

#define GPS_RX_PIN 16   // ESP32 pin that receives data FROM GPS TX
#define GPS_TX_PIN 17   // ESP32 pin that sends data TO GPS RX

#define LORA_FREQUENCY 433E6   // must match receiver

// Unique ID for this worker unit - change per device when you have multiple
#define DEVICE_ID 1

#define SEND_INTERVAL_MS 5000

// ---------------- Status codes ----------------
enum WorkerStatus : uint8_t {
  STATUS_NORMAL = 0,
  STATUS_SOS    = 1
};

// ---------------- Packet structure (5.5 adds lat/lon) ----------------
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

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.5: GPS Data Transmission ==="));

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // NEO-M8N default baud rate is 9600
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

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

  // Manual SOS button (active LOW) - send immediately
  if (digitalRead(BUTTON_PIN) == LOW) {
    sendPacket(STATUS_SOS);
    Serial.println(F("  >>> SOS button pressed! <<<"));
    delay(1000); // simple debounce
    return;
  }

  // Normal heartbeat status
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendPacket(STATUS_NORMAL);
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
  Serial.print(status == STATUS_SOS ? "SOS" : "NORMAL");
  Serial.print(F(" | GPS fix="));
  Serial.print(gps.location.isValid() ? "YES" : "NO");
  Serial.print(F(" | lat="));
  Serial.print(pkt.latitude, 6);
  Serial.print(F(" | lon="));
  Serial.println(pkt.longitude, 6);
}
