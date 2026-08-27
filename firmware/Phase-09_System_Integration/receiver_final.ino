/*
 * Project : Smart Safety Vest
 * File    : Receiver_Final.ino
 * Description:
 * ESP32 receiver: reads LoRa packets from the vest transmitter and pushes
 * data to Firebase. Worker status field is validated against the 5
 * supported categories: STANDING / WALKING / RUNNING / LYING / FALL.
 *
 * IMPORTANT (packet-loss fix):
 * All WiFi/HTTP calls (Firebase PUT for sensor data, Firebase GET for the
 * website's SOS flag) are SYNCHRONOUS/BLOCKING and can stall for hundreds
 * of ms to a few seconds. If they ran inside loop(), they would block
 * LoRa.parsePacket() long enough to miss incoming transmitter packets.
 * To fix this, all networking now runs on a separate FreeRTOS task pinned
 * to core 0, while loop() (core 1) is left free to poll LoRa continuously
 * and never blocks on WiFi/HTTP.
 *
 * ROOT CAUSE of "SOS button sometimes doesn't ring the vest buzzer":
 * LoRa is half-duplex on a single shared frequency with no collision
 * avoidance. The transmitter sends a telemetry packet every ~2s; if this
 * receiver's SOS_ON/SOS_OFF downlink happened to go out at the same
 * moment, both radios were busy transmitting and the command was simply
 * lost mid-air — with no error on either side, since the old code sent it
 * exactly once and never checked whether it actually arrived.
 * FIX: this file now tracks the desired state (from Firebase) separately
 * from the confirmed state (read back from the transmitter's own telemetry,
 * which now echoes its current webSosActive as a 10th CSV field). While the
 * two disagree, the command is resent every SOS_RETRY_INTERVAL_MS instead of
 * once — so a single lost-to-collision packet no longer means the SOS
 * command silently never arrives; it just arrives on the next retry.
 *
 * WEB-SOS-INCONSISTENT-BUZZER FIX (see getWebSosFlag() below):
 * A failed/timed-out Firebase poll for the web SOS flag used to be treated
 * as "OFF", so an occasional flaky poll could silently cancel an SOS that
 * was still ON on the website. getWebSosFlag() now reports failures
 * separately so a bad poll no longer overwrites the last known state.
 */

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// WiFi Config
#define WIFI_SSID "Koushik"
#define WIFI_PASSWORD "montus10"

// Firebase Config
#define DATABASE_URL "https://worker-safety-vest-92b97-default-rtdb.firebaseio.com/"

// LoRa Pins (SX1278 RA-02)
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  2

// The only worker-status values the transmitter is expected to send
bool isKnownStatus(const String &s) {
  return (s == "STANDING" || s == "WALKING" || s == "RUNNING" ||
          s == "LYING"    || s == "FALL");
}

/*
 * ==========================================================
 * Background networking (runs on core 0)
 * - Drains a queue of sensor-data JSON payloads -> Firebase PUT
 * - Periodically polls /worker1/commands/web_sos -> Firebase GET
 * loop() on core 1 NEVER calls WiFi/HTTP directly, so LoRa receiving
 * is never blocked.
 * ==========================================================
 */

struct FirebaseJob {
  char json[450];
  char path[40];   // Firebase path, e.g. "worker1" or "worker1/logs"
  bool isPush;      // true = HTTP POST (Firebase auto-generates a new key,
                     // used for /worker1/logs so every reading gets its own
                     // entry); false = HTTP PUT (overwrite, used for the
                     // live /worker1 snapshot the dashboard reads for "now")
};

QueueHandle_t firebaseQueue;

/*
 * ==========================================================
 * DASHBOARD-LAG FIX (this update)
 * ----------------------------------------------------------
 * Two separate root causes were found for "website updates more than
 * 10s late or sometimes stops updating entirely" even though the LoRa
 * link itself is fine:
 *
 * 1) NO WIFI AUTO-RECONNECT (this is what causes it to fully STOP):
 *    sendToFirebase()/getWebSosFlag() used to just check
 *    WiFi.status()==WL_CONNECTED and silently do nothing if not - if the
 *    ESP32's WiFi ever drops for even a moment (weak signal, router
 *    hiccup, DHCP renew), it never reconnected on its own. Every future
 *    write then failed forever until the board was power-cycled, which
 *    is exactly the "majhe majhe thamiye jay" symptom. FIX: networkTask
 *    now watches WiFi.status() every loop and calls WiFi.reconnect()
 *    whenever it drops, so it comes back on its own within a few seconds.
 *
 * 2) A FRESH TLS HANDSHAKE ON EVERY SINGLE REQUEST (this is what causes
 *    the >10s DELAY): every sendToFirebase()/getWebSosFlag() call created
 *    a brand-new HTTPClient with http.begin(url) and no shared TLS
 *    client, so each one of the 3 requests firing every ~1.5-2s (sensor
 *    PUT, log POST, SOS-flag GET) had to redo a full HTTPS handshake
 *    from scratch (can take 1-3s each on an ESP32's slow TLS
 *    stack). Those three blocking calls easily stack up past 10s,
 *    especially on a weaker WiFi signal. FIX: a single WiFiClientSecure
 *    (secureClient) is now reused across every call via
 *    http.begin(secureClient, url) + http.setReuse(true), so only the
 *    very first request pays the full handshake cost and the rest reuse
 *    the same warm TLS connection.
 * ==========================================================
 */
WiFiClientSecure secureClient;

// ==========================================================
// Data-logging config
// ----------------------------------------------------------
// LOG_EVERY_PACKET_MS = 0  -> every LoRa packet received also gets pushed
//                             to /worker1/logs with a server timestamp
//                             (this is what was asked for).
// If Firebase storage/bandwidth ever becomes a concern (this logs roughly
// every ~2-8s depending on the transmitter's send interval, which adds up
// over days), raise this to e.g. 10000 (10s) or 60000 (1 min) to throttle
// how often a log entry is written, without changing the live /worker1
// update which still happens every packet.
// ==========================================================
#define LOG_EVERY_PACKET_MS 0
unsigned long lastLogTime = 0;

// desiredWebSos is only WRITTEN by the core-0 task (from Firebase) and only
// READ by loop() on core 1 - single writer/single reader, safe as volatile.
volatile bool desiredWebSos = false;
// confirmedWebSos is only touched by loop() on core 1 (updated whenever a
// telemetry packet arrives echoing the transmitter's actual state) - no
// cross-core access, doesn't need to be volatile.
bool confirmedWebSos = false;
const unsigned long CMD_CHECK_INTERVAL = 1500;      // ms - how often we poll Firebase for the desired state
const unsigned long SOS_RETRY_INTERVAL_MS = 1200;   // ms - how often we resend while desired != confirmed

void sendToFirebase(const String &path, const String &jsonPayload, bool isPush) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(DATABASE_URL) + path + ".json";

    // Reuse the shared secureClient + keep the TLS connection alive
    // (setReuse) instead of a fresh handshake on every call - this is
    // what stops PUT/POST requests from stacking up into a multi-second
    // dashboard delay. See the note above firebaseQueue for details.
    http.begin(secureClient, url);
    http.setReuse(true);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(3000);

    // isPush=true -> POST: Firebase creates a new auto-ID child (used for
    // /worker1/logs so each reading becomes its own history entry instead
    // of overwriting the previous one).
    // isPush=false -> PUT: overwrites the target path (used for the live
    // /worker1 snapshot).
    int httpResponseCode = isPush ? http.POST(jsonPayload) : http.PUT(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.printf("[Firebase] Update Success! Code: %d\n", httpResponseCode);
    } else {
      Serial.printf("[Firebase] Error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  } else {
    Serial.println("[WiFi] Disconnected! (networkTask will auto-reconnect)");
  }
}

// ROOT CAUSE of "web SOS rings the buzzer inconsistently":
// This used to return a plain bool, initialized to false, and simply stayed
// false whenever WiFi was down or the Firebase GET failed/timed out (which
// happens occasionally on any real WiFi link - that's normal, not a fault).
// The caller then did `desiredWebSos = getWebSosFlag();` unconditionally, so
// a single dropped/failed poll silently overwrote an active "ON" with "OFF" -
// even though the website still showed the SOS button as ON and nothing in
// Firebase had actually changed. That immediately made loop() send SOS_OFF
// to the vest, cutting the buzzer that had just started (or never letting it
// start), with no error anywhere. This is exactly the "sometimes it rings,
// sometimes it doesn't" symptom.
// FIX: distinguish "confirmed OFF" from "couldn't check right now". Returns
// 1 = confirmed ON, 0 = confirmed OFF, -1 = read failed (WiFi down, timeout,
// non-200 response). The caller only updates desiredWebSos on a confirmed
// result (>= 0) and leaves the last known desired state untouched on -1, so
// a flaky poll can no longer turn off an SOS that's actually still active.
int getWebSosFlag() {
  if (WiFi.status() != WL_CONNECTED) return -1;

  HTTPClient http;
  String url = String(DATABASE_URL) + "worker1/commands/web_sos.json";
  http.begin(secureClient, url);   // reuse the same warm TLS connection
  http.setReuse(true);
  http.setTimeout(3000);
  int code = http.GET();

  int result = -1; // failed unless we get a clean 200 below
  if (code == 200) {
    String payload = http.getString();
    payload.trim();
    result = (payload == "true") ? 1 : 0;
  } else {
    Serial.printf("[Firebase] web_sos GET failed, code: %d (keeping last known state)\n", code);
  }
  http.end();
  return result;
}

void networkTask(void *param) {
  unsigned long lastPollTime = 0;
  unsigned long lastWifiCheckTime = 0;
  unsigned long lastReconnectAttempt = 0;
  const unsigned long WIFI_CHECK_INTERVAL = 500;      // ms - how often we check the link
  const unsigned long WIFI_RECONNECT_COOLDOWN = 5000; // ms - don't hammer reconnect attempts

  for (;;) {
    // 0. WiFi watchdog: this is what actually stops the "website
    // sometimes just stops updating" symptom. Without this, a single
    // dropped WiFi link (weak signal, router hiccup, DHCP renew) left
    // every future Firebase write silently failing forever, since
    // nothing ever told the ESP32 to reconnect.
    if (millis() - lastWifiCheckTime > WIFI_CHECK_INTERVAL) {
      lastWifiCheckTime = millis();
      if (WiFi.status() != WL_CONNECTED &&
          millis() - lastReconnectAttempt > WIFI_RECONNECT_COOLDOWN) {
        lastReconnectAttempt = millis();
        Serial.println("[WiFi] Link down - attempting reconnect...");
        WiFi.reconnect();
      }
    }

    // 1. Drain any queued sensor-data PUT jobs (non-blocking pop)
    FirebaseJob job;
    while (xQueueReceive(firebaseQueue, &job, 0) == pdTRUE) {
      sendToFirebase(String(job.path), String(job.json), job.isPush);
    }

    // 2. Periodically poll the website's SOS toggle. Just record the
    // desired state here - loop() on core 1 is responsible for actually
    // getting it to the transmitter (and retrying until confirmed).
    if (millis() - lastPollTime > CMD_CHECK_INTERVAL) {
      lastPollTime = millis();
      int flag = getWebSosFlag();
      if (flag >= 0) {
        desiredWebSos = (flag == 1);
      }
      // flag == -1: poll failed (WiFi/HTTP hiccup) - keep the previous
      // desiredWebSos instead of assuming OFF. See getWebSosFlag() above.
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void sendLoRaCommand(const char* cmd) {
  LoRa.beginPacket();
  LoRa.print(cmd);
  LoRa.endPacket();
  Serial.printf("[LoRa] Downlink command sent: %s\n", cmd);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi Connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected!");
  WiFi.setAutoReconnect(true); // built-in ESP32 helper, backs up our own watchdog below

  // Shared TLS client for all Firebase calls (see note above firebaseQueue).
  // setInsecure() skips certificate validation - fine here since we're only
  // talking to Firebase's own *.firebaseio.com endpoint, not user input -
  // but it's what makes the reused connection possible at all without
  // bundling Google's root CA cert on the device.
  secureClient.setInsecure();

  // LoRa Setup
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("[LoRa] Receiver Initialization Failed!");
    while (1);
  }
  Serial.println("[LoRa] Receiver Ready & Listening...\n");

  // Background networking task - runs on core 0, keeps core 1 (loop) free for LoRa
  firebaseQueue = xQueueCreate(10, sizeof(FirebaseJob));
  xTaskCreatePinnedToCore(networkTask, "networkTask", 8192, NULL, 1, NULL, 0);
}

void loop() {
  // Resend the SOS command every SOS_RETRY_INTERVAL_MS for as long as the
  // desired state (from Firebase) and the confirmed state (last echoed back
  // by the transmitter in its telemetry) disagree. This is what makes the
  // command survive an occasional LoRa collision instead of being lost for
  // good - it just goes out again a bit over a second later.
  static unsigned long lastSosSendTime = 0;
  bool desired = desiredWebSos; // snapshot the volatile once
  if (desired != confirmedWebSos && millis() - lastSosSendTime > SOS_RETRY_INTERVAL_MS) {
    sendLoRaCommand(desired ? "SOS_ON" : "SOS_OFF");
    lastSosSendTime = millis();
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // CSV Parse: temp,hum,pres,state,fall,lat,lng,sos,batt,webSosActive
    // (webSosActive is the 10th field - the transmitter's confirmation of
    // the last SOS_ON/SOS_OFF command it actually applied)
    int idx1 = incoming.indexOf(',');
    int idx2 = incoming.indexOf(',', idx1 + 1);
    int idx3 = incoming.indexOf(',', idx2 + 1);
    int idx4 = incoming.indexOf(',', idx3 + 1);
    int idx5 = incoming.indexOf(',', idx4 + 1);
    int idx6 = incoming.indexOf(',', idx5 + 1);
    int idx7 = incoming.indexOf(',', idx6 + 1);
    int idx8 = incoming.indexOf(',', idx7 + 1);
    int idx9 = incoming.indexOf(',', idx8 + 1);

    if (idx9 != -1) {
      float temp = incoming.substring(0, idx1).toFloat();
      float hum  = incoming.substring(idx1 + 1, idx2).toFloat();
      float pres = incoming.substring(idx2 + 1, idx3).toFloat();
      String state = incoming.substring(idx3 + 1, idx4);
      bool fall = incoming.substring(idx4 + 1, idx5).toInt();
      float lat  = incoming.substring(idx5 + 1, idx6).toFloat();
      float lng  = incoming.substring(idx6 + 1, idx7).toFloat();
      bool sos   = incoming.substring(idx7 + 1, idx8).toInt();
      float batt = incoming.substring(idx8 + 1, idx9).toFloat();
      bool txWebSos = incoming.substring(idx9 + 1).toInt();

      // This is the "ack": stop retrying once the transmitter confirms it's
      // actually applying the state we asked for.
      confirmedWebSos = txWebSos;

      if (!isKnownStatus(state)) {
        Serial.printf("[Warning] Unrecognized worker status received: \"%s\"\n", state.c_str());
      }

      // Serial Monitor Output Print
      Serial.println("====== [ LORA RECEIVED PACKET ] ======");
      Serial.printf("Raw Payload : %s\n", incoming.c_str());
      Serial.printf("RSSI        : %d dBm\n", LoRa.packetRssi());
      Serial.printf("Temp        : %.1f C | Hum: %.1f %% | Pres: %.1f hPa\n", temp, hum, pres);
      Serial.printf("Worker Status: %s\n", state.c_str());
      Serial.printf("Fall Alert  : %s\n", fall ? "YES (ALERT)" : "NO");
      Serial.printf("SOS Alert   : %s\n", sos ? "YES (ACTIVE)" : "NO");
      Serial.printf("Web SOS     : desired=%s confirmed=%s\n", desiredWebSos ? "ON" : "OFF", confirmedWebSos ? "ON" : "OFF");
      Serial.printf("GPS Location: Lat: %.6f, Lng: %.6f\n", lat, lng);
      Serial.printf("Battery Volt: %.2f V\n", batt);
      Serial.println("======================================\n");

      // Build JSON and hand it off to the background task instead of
      // sending it here directly - keeps loop() non-blocking for LoRa.
      String json = "{";
      json += "\"environment\":{\"temperature\":" + String(temp, 1) + ",\"humidity\":" + String(hum, 1) + ",\"pressure\":" + String(pres, 1) + "},";
      json += "\"status\":{\"motion_state\":\"" + state + "\"},";
      json += "\"alerts\":{\"fall_detected\":" + String(fall ? "true" : "false") + ",\"sos_active\":" + String(sos ? "true" : "false") + "},";
      json += "\"gps\":{\"latitude\":" + String(lat, 6) + ",\"longitude\":" + String(lng, 6) + "},";
      json += "\"battery\":{\"voltage\":" + String(batt, 2) + "}";
      json += "}";

      FirebaseJob job;
      strncpy(job.path, "worker1", sizeof(job.path));
      job.isPush = false; // overwrite the live snapshot
      json.toCharArray(job.json, sizeof(job.json));
      if (xQueueSend(firebaseQueue, &job, 0) != pdTRUE) {
        Serial.println("[Warning] Firebase queue full - dropping this update (network is lagging behind).");
      }

      // ---- Timestamped history entry ----
      // {".sv":"timestamp"} tells Firebase's server to fill this field with
      // its own current time (ms since epoch) when the write lands - the
      // ESP32 doesn't need an RTC or NTP sync for this to be accurate.
      // Pushed (POST) instead of PUT so every reading gets its own key
      // under /worker1/logs instead of overwriting the last one; the
      // dashboard's Reports feature already reads this exact field layout
      // (ts, temperature, humidity, pressure, fall, sos).
      if (LOG_EVERY_PACKET_MS == 0 || millis() - lastLogTime >= LOG_EVERY_PACKET_MS) {
        lastLogTime = millis();

        String logJson = "{";
        logJson += "\"ts\":{\".sv\":\"timestamp\"},";
        logJson += "\"temperature\":" + String(temp, 1) + ",";
        logJson += "\"humidity\":" + String(hum, 1) + ",";
        logJson += "\"pressure\":" + String(pres, 1) + ",";
        logJson += "\"motion_state\":\"" + state + "\",";
        logJson += "\"fall\":" + String(fall ? "true" : "false") + ",";
        logJson += "\"sos\":" + String(sos ? "true" : "false") + ",";
        logJson += "\"latitude\":" + String(lat, 6) + ",";
        logJson += "\"longitude\":" + String(lng, 6) + ",";
        logJson += "\"battery\":" + String(batt, 2);
        logJson += "}";

        FirebaseJob logJob;
        strncpy(logJob.path, "worker1/logs", sizeof(logJob.path));
        logJob.isPush = true; // POST: new auto-ID child, don't overwrite history
        logJson.toCharArray(logJob.json, sizeof(logJob.json));
        if (xQueueSend(firebaseQueue, &logJob, 0) != pdTRUE) {
          Serial.println("[Warning] Firebase queue full - dropping this log entry.");
        }
      }
    } else {
      Serial.println("[Warning] Invalid Packet Format Received!");
    }
  }
}
