/*
 * Project : Smart Safety Vest
 * File    : Receiver_Final.ino
 * Description:
 * ESP32 receiver: reads LoRa packets from the vest transmitter and pushes
 * data to Firebase. Worker status field is validated against the 5
 * supported categories: STANDING / WALKING / RUNNING / LYING / FALL.
 *
 * ==========================================================================
 * LIVE-DATA REWORK (why the dashboard used to stall, and what changed)
 * ==========================================================================
 * The transmitter now sends one packet per second. The old upload pipeline
 * could not keep up with that, and three separate faults made the dashboard
 * freeze for long stretches:
 *
 * 1) TWO BLOCKING HTTPS REQUESTS PER PACKET.
 *    Every packet fired a PUT (live snapshot) *and* a POST (history log).
 *    Each request is a synchronous round trip that takes 150 ms on good WiFi
 *    and several seconds on a weak link. At one packet per second there is
 *    simply not enough time, so the 10-slot queue filled up and entries were
 *    dropped ("Firebase queue full" warnings).
 *    FIX: history entries are now BATCHED. Up to LOG_BATCH_MAX entries are
 *    merged into a single Firebase PATCH every LOG_FLUSH_INTERVAL_MS, so
 *    roughly 10 readings cost one request instead of ten. Nothing is thrown
 *    away - the batch just leaves a moment later.
 *
 * 2) STALE LIVE DATA SITTING BEHIND A BACKLOG.
 *    The live snapshot used the same FIFO queue as the logs, so after a
 *    network hiccup the dashboard would slowly replay a queue of readings
 *    that were already tens of seconds old.
 *    FIX: the live snapshot now lives in its own one-slot queue written with
 *    xQueueOverwrite(). A newer reading simply replaces an unsent older one,
 *    so "live" always means the newest packet received, never a backlog.
 *
 * 3) NOTHING EVER RECOVERED FROM A HALF-DEAD CONNECTION.
 *    A TLS session that the server had quietly closed, or a WiFi link that
 *    associated but could not route, left every request failing forever with
 *    no escalation beyond WiFi.reconnect().
 *    FIX: a layered watchdog (see connectionWatchdog) escalates on its own -
 *    reset the TLS session, then force a full WiFi re-association, then
 *    reboot the board as a last resort. Also WiFi.setSleep(false) is now set,
 *    which removes the modem-sleep latency spikes that made individual
 *    requests randomly take hundreds of ms longer than they should.
 *
 * DATA COMPLETENESS
 * Readings are buffered in RAM (LOG_QUEUE_LEN entries) while the network is
 * unavailable and flushed once it returns, so an outage of up to about
 * LOG_QUEUE_LEN seconds is ridden out with no missing history. Failed
 * batches are pushed back onto the front of the queue rather than discarded.
 *
 * The JSON written to /worker1 and /worker1/logs is unchanged, so the
 * website needs no modification.
 */

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// WiFi Config
#define WIFI_SSID "Koushik"
#define WIFI_PASSWORD "montus10"

// Firebase Config
#define DATABASE_URL "https://worker-safety-vest-92b97-default-rtdb.firebaseio.com/"
#define LIVE_PATH "worker1"
#define LOG_PATH  "worker1/logs"

// LoRa Pins (SX1278 RA-02)
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  2

// ==========================================================
// Upload pipeline tuning
// ----------------------------------------------------------
// LOG_QUEUE_LEN        : how many readings can be held in RAM while the
//                        network is down. At one packet per second this is
//                        also roughly how many seconds of outage can pass
//                        with zero history loss. Each slot is ~300 bytes, so
//                        90 slots cost ~27 KB of heap - raise it only if you
//                        watch the free-heap figure on the serial monitor.
// LOG_BATCH_MAX        : how many readings are merged into one PATCH.
// LOG_FLUSH_INTERVAL_MS: how often a batch is sent. Lower = fresher history
//                        but more requests; 2000 ms keeps the request rate
//                        at a level the ESP32's TLS stack handles easily.
// ==========================================================
#define LOG_QUEUE_LEN          90
#define LOG_BATCH_MAX          12
#define LOG_FLUSH_INTERVAL_MS  2000
#define HTTP_TIMEOUT_MS        5000

// LOG_EVERY_MS = 0 -> every single packet is written to /worker1/logs, i.e. a
// full one-second-resolution history (this is what was asked for). Be aware
// that one entry per second is ~86,400 entries/day; if Firebase storage ever
// becomes a concern, set this to e.g. 5000 (one entry per 5 s) - the LIVE
// /worker1 snapshot the dashboard reads keeps updating every second either
// way, only the stored history gets thinner.
#define LOG_EVERY_MS 0
unsigned long lastLogTime = 0;

// The only worker-status values the transmitter is expected to send
bool isKnownStatus(const String &s) {
  return (s == "STANDING" || s == "WALKING" || s == "RUNNING" ||
          s == "LYING"    || s == "FALL");
}

// ==========================================================
// Shared state between the two cores
// ----------------------------------------------------------
// loop() runs on core 1 and does nothing but poll LoRa and hand work over.
// networkTask runs on core 0 and owns every WiFi/HTTP call, so a slow
// Firebase round trip can never delay packet reception.
// ==========================================================

struct LiveSnapshot {
  char json[420];
};

struct LogEntry {
  char key[20];   // Firebase child key: zero-padded epoch ms (sorts chronologically)
  char json[280];
};

QueueHandle_t liveQueue;  // length 1, written with xQueueOverwrite (newest wins)
QueueHandle_t logQueue;   // FIFO history buffer, drained in batches

// Single TLS client reused across every request. A fresh HTTPS handshake per
// request costs 1-3 s on an ESP32, which on its own is enough to push the
// dashboard past a 10 s delay - so the connection is kept warm instead.
WiFiClientSecure secureClient;

// Health tracking (read/written only by networkTask on core 0 unless noted)
volatile unsigned long lastFirebaseOkMs = 0;
volatile unsigned long lastPacketMs = 0;   // written by loop() on core 1
volatile uint32_t droppedLogEntries = 0;
volatile uint32_t missedPackets = 0;       // written by loop() on core 1

/*
 * ==========================================================
 * Time sync
 * ----------------------------------------------------------
 * History entries are written with an explicit key instead of Firebase's
 * auto-generated push ID, because batching means one request carries many
 * children and each needs its own key. Using epoch milliseconds keeps those
 * keys unique AND in chronological order (a 13-digit number sorts correctly
 * as a string), which is exactly what a push ID would have given us.
 * The "ts" FIELD is still {".sv":"timestamp"} - filled by Firebase's own
 * server clock - so the dashboard's Reports feature is unaffected and the
 * displayed times stay accurate even if NTP is a little off.
 * ==========================================================
 */
bool timeIsValid() {
  return time(nullptr) > 1700000000; // sanity floor: any time after Nov 2023
}

void makeLogKey(char *out, size_t n) {
  static uint64_t lastKeyMs = 0;

  if (timeIsValid()) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ms = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
    if (ms <= lastKeyMs) ms = lastKeyMs + 1; // guarantee uniqueness + ordering
    lastKeyMs = ms;
    snprintf(out, n, "%013llu", (unsigned long long)ms);
  } else {
    // NTP hasn't landed yet (normally only the first second or two after
    // boot). A leading '0' keeps these sorted ahead of every real key.
    snprintf(out, n, "0%012lu", (unsigned long)millis());
  }
}

/*
 * ==========================================================
 * Firebase transport (core 0 only)
 * ==========================================================
 */
int firebaseRequest(const char *path, const char *method, const String &payload) {
  HTTPClient http;
  String url = String(DATABASE_URL) + path + ".json";

  if (!http.begin(secureClient, url)) return -1;

  http.setReuse(true);                    // keep the TLS session warm
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  int code = http.sendRequest(method, (uint8_t *)payload.c_str(), payload.length());
  http.end();

  if (code == 200) lastFirebaseOkMs = millis();
  return code;
}

// One inline retry on a fresh TLS session. The most common failure by far is
// a keep-alive connection the server closed while we were idle; stopping the
// client forces a clean handshake and the retry then succeeds immediately.
bool firebaseSend(const char *path, const char *method, const String &payload) {
  int code = firebaseRequest(path, method, payload);
  if (code == 200) return true;

  Serial.printf("[Firebase] %s %s failed (code %d) - retrying on a fresh connection\n",
                method, path, code);
  secureClient.stop();
  vTaskDelay(pdMS_TO_TICKS(60));

  code = firebaseRequest(path, method, payload);
  if (code == 200) return true;

  Serial.printf("[Firebase] %s %s failed again (code %d)\n", method, path, code);
  secureClient.stop();
  return false;
}

// Live snapshot: newest reading only, never retried (a fresher one is always
// a second away, and replaying stale "live" data is worse than skipping it).
void pushLiveSnapshot() {
  LiveSnapshot snap;
  if (xQueueReceive(liveQueue, &snap, 0) != pdTRUE) return;
  firebaseSend(LIVE_PATH, "PUT", String(snap.json));
}

// History: many readings merged into one PATCH. On failure the whole batch
// goes back to the FRONT of the queue so nothing is lost; ordering inside
// Firebase comes from the keys, not from send order.
void flushLogBatch() {
  static LogEntry batch[LOG_BATCH_MAX];
  int n = 0;
  while (n < LOG_BATCH_MAX && xQueueReceive(logQueue, &batch[n], 0) == pdTRUE) n++;
  if (n == 0) return;

  String body = "{";
  for (int i = 0; i < n; i++) {
    if (i) body += ",";
    body += "\"";
    body += batch[i].key;
    body += "\":";
    body += batch[i].json;
  }
  body += "}";

  if (firebaseSend(LOG_PATH, "PATCH", body)) {
    Serial.printf("[Firebase] History batch written (%d entries, %u still buffered)\n",
                  n, (unsigned)uxQueueMessagesWaiting(logQueue));
  } else {
    for (int i = n - 1; i >= 0; i--) {
      if (xQueueSendToFront(logQueue, &batch[i], 0) != pdTRUE) droppedLogEntries++;
    }
    Serial.printf("[Firebase] Batch failed - %d entries kept for the next attempt\n", n);
  }
}

/*
 * ==========================================================
 * Watchdogs (core 0 only)
 * ==========================================================
 */
void wifiWatchdog() {
  static unsigned long lastCheck = 0;
  static unsigned long lastAttempt = 0;

  if (millis() - lastCheck < 500) return;
  lastCheck = millis();

  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastAttempt < 5000) return; // don't hammer

  lastAttempt = millis();
  Serial.println("[WiFi] Link down - reconnecting...");
  WiFi.reconnect();
}

// Escalating recovery for the case that actually caused long freezes: WiFi
// says "connected" but requests still fail, so nothing below ever noticed
// anything was wrong. Only runs when there is real work stuck in the queue,
// so a vest that is simply switched off never triggers a reboot.
void connectionWatchdog() {
  static unsigned long lastTlsReset = 0;
  static unsigned long lastHardReset = 0;

  if (uxQueueMessagesWaiting(logQueue) == 0) return; // nothing pending, nothing wrong
  unsigned long stalled = millis() - lastFirebaseOkMs;

  if (stalled > 30000 && millis() - lastTlsReset > 30000) {
    lastTlsReset = millis();
    Serial.println("[Recovery] No successful write for 30 s - resetting the TLS session.");
    secureClient.stop();
  }

  if (stalled > 90000 && millis() - lastHardReset > 90000) {
    lastHardReset = millis();
    Serial.println("[Recovery] No successful write for 90 s - forcing a full WiFi re-association.");
    secureClient.stop();
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(400));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(false);
  }

  if (stalled > 240000) {
    Serial.println("[Recovery] Still no successful write after 4 minutes - rebooting.");
    Serial.flush();
    ESP.restart();
  }
}

void networkTask(void *param) {
  unsigned long lastFlush = 0;

  for (;;) {
    wifiWatchdog();

    if (WiFi.status() == WL_CONNECTED) {
      // 1. Live snapshot first - this is what the dashboard shows as "now".
      pushLiveSnapshot();

      // 2. History batch, either on schedule or early if the buffer is
      //    filling faster than the flush interval drains it.
      if (millis() - lastFlush >= LOG_FLUSH_INTERVAL_MS ||
          uxQueueMessagesWaiting(logQueue) >= LOG_BATCH_MAX) {
        lastFlush = millis();
        flushLogBatch();
      }

      connectionWatchdog();
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/*
 * ==========================================================
 * Setup
 * ==========================================================
 */
void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi. setSleep(false) is important here: with modem sleep on (the
  // default) the radio parks between beacons and individual requests pick up
  // random extra latency, which is a large part of the "sometimes it takes
  // forever" behaviour.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(500);
    Serial.print(".");
  }
  // Deliberately NOT an infinite wait: if the router is down at power-up the
  // board still boots, keeps receiving LoRa into the buffer, and uploads
  // everything once wifiWatchdog() gets the link back.
  Serial.println(WiFi.status() == WL_CONNECTED ? "\n[WiFi] Connected!"
                                               : "\n[WiFi] Not connected yet - continuing, watchdog will retry.");

  // Clock for the history keys (non-blocking; SNTP fills it in the background)
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  // setInsecure() skips certificate validation - acceptable here since the
  // only endpoint contacted is Firebase's own *.firebaseio.com host - and it
  // is what lets a single connection be reused without bundling a root CA.
  secureClient.setInsecure();

  // LoRa Setup
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("[LoRa] Receiver Initialization Failed!");
    while (1);
  }
  LoRa.enableCrc(); // must match the transmitter - drops corrupted packets
  Serial.println("[LoRa] Receiver Ready & Listening...\n");

  liveQueue = xQueueCreate(1, sizeof(LiveSnapshot));
  logQueue  = xQueueCreate(LOG_QUEUE_LEN, sizeof(LogEntry));

  lastFirebaseOkMs = millis();
  lastPacketMs = millis();

  // Networking runs on core 0 with a generous stack (a full log batch is
  // assembled on this task's stack). loop() keeps core 1 to itself.
  xTaskCreatePinnedToCore(networkTask, "networkTask", 16384, NULL, 1, NULL, 0);
}

/*
 * ==========================================================
 * Loop - core 1, LoRa only, never blocks on the network
 * ==========================================================
 */
void loop() {
  // Quiet-link notice. Not an error on its own (the vest may be off), but it
  // separates "the radio link stopped" from "the upload stopped" at a glance.
  static unsigned long lastQuietWarn = 0;
  if (millis() - lastPacketMs > 15000 && millis() - lastQuietWarn > 15000) {
    lastQuietWarn = millis();
    Serial.printf("[LoRa] No packet for %lu s - transmitter off or out of range?\n",
                  (millis() - lastPacketMs) / 1000);
  }

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String incoming = "";
  while (LoRa.available()) incoming += (char)LoRa.read();

  // CSV Parse: temp,hum,pres,state,fall,lat,lng,sos,batt,seq
  // (seq is diagnostics only - it is never uploaded)
  int idx1 = incoming.indexOf(',');
  int idx2 = incoming.indexOf(',', idx1 + 1);
  int idx3 = incoming.indexOf(',', idx2 + 1);
  int idx4 = incoming.indexOf(',', idx3 + 1);
  int idx5 = incoming.indexOf(',', idx4 + 1);
  int idx6 = incoming.indexOf(',', idx5 + 1);
  int idx7 = incoming.indexOf(',', idx6 + 1);
  int idx8 = incoming.indexOf(',', idx7 + 1);
  int idx9 = incoming.indexOf(',', idx8 + 1);

  if (idx8 == -1) {
    Serial.println("[Warning] Invalid Packet Format Received!");
    return;
  }

  lastPacketMs = millis();

  float temp  = incoming.substring(0, idx1).toFloat();
  float hum   = incoming.substring(idx1 + 1, idx2).toFloat();
  float pres  = incoming.substring(idx2 + 1, idx3).toFloat();
  String state = incoming.substring(idx3 + 1, idx4);
  bool fall   = incoming.substring(idx4 + 1, idx5).toInt();
  float lat   = incoming.substring(idx5 + 1, idx6).toFloat();
  float lng   = incoming.substring(idx6 + 1, idx7).toFloat();
  bool sos    = incoming.substring(idx7 + 1, idx8).toInt();
  float batt  = (idx9 == -1) ? incoming.substring(idx8 + 1).toFloat()
                             : incoming.substring(idx8 + 1, idx9).toFloat();
  unsigned long seq = (idx9 == -1) ? 0 : incoming.substring(idx9 + 1).toInt();

  // Over-the-air loss detection. Nothing is uploaded from this - it just
  // tells you on the serial monitor whether a gap in the history came from
  // the radio link or from the upload side.
  static unsigned long lastSeq = 0;
  if (seq && lastSeq && seq > lastSeq + 1) {
    unsigned long lost = seq - lastSeq - 1;
    missedPackets += lost;
    Serial.printf("[LoRa] %lu packet(s) lost over the air (seq %lu -> %lu)\n",
                  lost, lastSeq, seq);
  }
  if (seq) lastSeq = seq;

  if (!isKnownStatus(state)) {
    Serial.printf("[Warning] Unrecognized worker status received: \"%s\"\n", state.c_str());
  }

  // Serial Monitor Output Print
  Serial.println("====== [ LORA RECEIVED PACKET ] ======");
  Serial.printf("Seq / RSSI  : #%lu | %d dBm\n", seq, LoRa.packetRssi());
  Serial.printf("Temp        : %.1f C | Hum: %.1f %% | Pres: %.1f hPa\n", temp, hum, pres);
  Serial.printf("Worker Status: %s\n", state.c_str());
  Serial.printf("Fall Alert  : %s\n", fall ? "YES (ALERT)" : "NO");
  Serial.printf("SOS Alert   : %s\n", sos ? "YES (ACTIVE)" : "NO");
  Serial.printf("GPS Location: Lat: %.6f, Lng: %.6f\n", lat, lng);
  Serial.printf("Battery Volt: %.2f V\n", batt);
  Serial.printf("Buffered    : %u entries | air-loss total: %u | free heap: %u B\n",
                (unsigned)uxQueueMessagesWaiting(logQueue), (unsigned)missedPackets,
                (unsigned)ESP.getFreeHeap());
  Serial.println("======================================\n");

  // ---- Live snapshot (dashboard's "now") ----
  // Identical JSON to before, so the website needs no changes.
  String json = "{";
  json += "\"environment\":{\"temperature\":" + String(temp, 1) + ",\"humidity\":" + String(hum, 1) + ",\"pressure\":" + String(pres, 1) + "},";
  json += "\"status\":{\"motion_state\":\"" + state + "\"},";
  json += "\"alerts\":{\"fall_detected\":" + String(fall ? "true" : "false") + ",\"sos_active\":" + String(sos ? "true" : "false") + "},";
  json += "\"gps\":{\"latitude\":" + String(lat, 6) + ",\"longitude\":" + String(lng, 6) + "},";
  json += "\"battery\":{\"voltage\":" + String(batt, 2) + "}";
  json += "}";

  LiveSnapshot snap;
  json.toCharArray(snap.json, sizeof(snap.json));
  // Overwrite rather than enqueue: if the previous snapshot hasn't gone out
  // yet, this newer one replaces it. "Live" can never fall behind.
  xQueueOverwrite(liveQueue, &snap);

  // ---- History entry (every single packet, nothing skipped) ----
  if (LOG_EVERY_MS != 0 && millis() - lastLogTime < LOG_EVERY_MS) return;
  lastLogTime = millis();

  // {".sv":"timestamp"} still makes Firebase's own server stamp the time, so
  // no RTC is needed on the ESP32 and the Reports view is unchanged.
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

  LogEntry entry;
  makeLogKey(entry.key, sizeof(entry.key));
  logJson.toCharArray(entry.json, sizeof(entry.json));

  if (xQueueSend(logQueue, &entry, 0) != pdTRUE) {
    // Buffer full only after a very long outage. Drop the OLDEST reading to
    // make room for the newest - recent data is the more useful of the two.
    LogEntry discarded;
    if (xQueueReceive(logQueue, &discarded, 0) == pdTRUE) droppedLogEntries++;
    if (xQueueSend(logQueue, &entry, 0) != pdTRUE) droppedLogEntries++;
    Serial.printf("[Warning] History buffer full after a long outage - oldest entry dropped (total dropped: %u)\n",
                  (unsigned)droppedLogEntries);
  }
}
