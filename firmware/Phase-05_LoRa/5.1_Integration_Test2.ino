/*
  ============================================================
  PHASE 5.1 - SX1278 RA-02 Integration Test
  ============================================================
  Purpose: ONLY verify that the SX1278 RA-02 module is wired
  correctly and communicates with the Arduino over SPI.
  No GPS, no sensors, no packet logic yet - just a basic init
  check and a simple "ping" message every 5 seconds.

  If this sketch prints "LoRa init succeeded!" - 5.1 is done,
  move on to 5.2 (Transmitter Setup).

  If it prints "LoRa init failed" - check wiring, VCC (must be
  3.3V not 5V), and that all pins match your board.

  Board: ESP32 (any dev board - DOIT ESP32 DEVKIT V1, WROOM32, etc.)

  Wiring (ESP32):
    VCC   -> 3.3V
    GND   -> GND
    SCK   -> GPIO 18
    MISO  -> GPIO 19
    MOSI  -> GPIO 23
    NSS   -> GPIO 5
    RESET -> GPIO 14
    DIO0  -> GPIO 26
  ============================================================
*/

#include <SPI.h>
#include <LoRa.h>

// ---------------- ESP32 pin mapping ----------------
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

// ESP32 default VSPI pins (used automatically by SPI.begin() below):
// SCK -> GPIO18, MISO -> GPIO19, MOSI -> GPIO23

// Set to match your region's LoRa frequency for the RA-02 variant you have:
// 433E6 (Asia - common in BD), 868E6 (Europe), 915E6 (North America)
#define LORA_FREQUENCY 433E6

unsigned long lastSend = 0;
int counter = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println();
  Serial.println(F("=== Phase 5.1: SX1278 RA-02 Integration Test (ESP32) ==="));

  // Explicit SPI pin setup for ESP32 (SCK, MISO, MOSI, SS)
  SPI.begin(18, 19, 23, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa init FAILED."));
    Serial.println(F("Check: wiring, 3.3V power, correct frequency, loose jumpers."));
    while (true) {
      delay(1000); // halt here so the failure is obvious
    }
  }

  Serial.println(F("LoRa init SUCCEEDED!"));
  Serial.println(F("Module is communicating over SPI correctly."));
  Serial.println(F("Sending a test packet every 5 seconds..."));
}

void loop() {
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    counter++;

    Serial.print(F("Sending test packet #"));
    Serial.println(counter);

    LoRa.beginPacket();
    LoRa.print("Hello from SX1278 #");
    LoRa.print(counter);
    LoRa.endPacket();
  }

  // Also print anything received (in case a second module is listening)
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print(F("Received: "));
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }
    Serial.print(F("  | RSSI: "));
    Serial.println(LoRa.packetRssi());
  }
}
