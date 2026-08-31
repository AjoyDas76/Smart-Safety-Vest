package com.smartvest.command.util

import android.os.Handler
import android.os.Looper
import android.util.Log
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener

/**
 * Single source of truth for the vest's live sensor readings.
 *
 * Listens to Firebase Realtime Database at "<WORKER_PATH>". If no update is
 * received for OFFLINE_TIMEOUT_MS (6 seconds), the vest is considered OFFLINE and
 * all screens are told to stop showing values instead of any default/fake number.
 *
 * Matches the real database structure (confirmed from the Firebase Console):
 *   worker1/
 *     alerts/
 *       fall_detected: Boolean
 *       sos_active: Boolean
 *     battery/
 *       voltage: Double
 *     environment/
 *       humidity: Double
 *       pressure: Double
 *       temperature: Double
 *     gps/
 *       latitude: Double
 *       longitude: Double
 *     status/
 *       motion_state: String
 *
 * NOTE: If you add more workers later (worker2, worker3, ...), WORKER_PATH needs
 * to become dynamic (e.g. picked from a logged-in worker's profile) instead of
 * this hardcoded "worker1".
 *
 * IMPORTANT — if data still doesn't reach the app, check Logcat with tag
 * "VestTelemetry" first. Remaining common causes (Firebase Console side, not in
 * this file):
 *   1. Realtime Database RULES deny read to signed-in users, e.g. ".read": false.
 *      If your hardware writes with the Admin SDK / a database secret, writes
 *      bypass rules entirely — so writes can keep succeeding while the app's
 *      read is silently rejected. Fix: Realtime Database -> Rules, allow
 *      ".read": "auth != null" (or whatever matches your logged-in users).
 *   2. Your Realtime Database instance's URL (shown at the top of the console)
 *      must match "firebase_url" in google-services.json.
 */
object VestTelemetryManager {

    data class Telemetry(
        val temperature: Double?,
        val humidity: Double?,
        val pressure: Double?,
        val batteryVoltage: Double?,
        val fallDetected: Boolean?,
        val sosActive: Boolean?,
        val motionState: String?,
        val latitude: Double?,
        val longitude: Double?,
        val isOnline: Boolean
    )

    /** One logged reading, used by the Report screen (charts + Excel export). */
    data class HistoryRecord(
        val timestampMillis: Long,
        val temperature: Double?,
        val humidity: Double?,
        val pressure: Double?,
        val motionState: String?,
        val fallDetected: Boolean?,
        val sosActive: Boolean?
    )

    private const val TAG = "VestTelemetry"

    // Real path confirmed from the Firebase Console: root -> worker1 -> {alerts,
    // battery, environment, gps, status}. Was "vest_data" (wrong node — this is
    // the reason data never showed up in the app even though Firebase itself
    // was updating fine).
    private const val WORKER_PATH = "worker1"

    // Where each logged reading is stored, so the Report screen (Daily/Weekly/
    // Monthly charts + Excel export) has real historical data to read instead
    // of nothing — "worker1" only ever held the *current* instantaneous
    // reading, with no history anywhere.
    private const val HISTORY_PATH = "$WORKER_PATH/history"

    // One logged point per minute is enough resolution for daily/weekly/monthly
    // reporting while keeping the amount of written/read data reasonable
    // (hardware reports every ~10-15s, which would otherwise be ~6x more writes
    // than needed).
    private const val HISTORY_LOG_INTERVAL_MS = 60_000L
    private var lastHistoryLogAt = 0L

    // Set back to 6s per explicit request. Note: the hardware reports roughly
    // every 10-15s, so with a 6s window the UI may flicker to OFFLINE between
    // real updates even when the vest is fine — flagged once here for the record.
    private const val OFFLINE_TIMEOUT_MS = 6000L

    private val handler = Handler(Looper.getMainLooper())
    private val listeners = mutableListOf<(Telemetry) -> Unit>()

    private var temperature: Double? = null
    private var humidity: Double? = null
    private var pressure: Double? = null
    private var batteryVoltage: Double? = null
    private var fallDetected: Boolean? = null
    private var sosActive: Boolean? = null
    private var motionState: String? = null
    private var latitude: Double? = null
    private var longitude: Double? = null
    private var isOnline = false
    private var started = false

    private val offlineWatchdog = Runnable {
        Log.w(TAG, "No data received for ${OFFLINE_TIMEOUT_MS}ms — marking vest OFFLINE")
        isOnline = false
        notifyListeners()
    }

    /** Call in onResume(). The listener is immediately given the current known state. */
    fun addListener(listener: (Telemetry) -> Unit) {
        listeners.add(listener)
        start()
        listener(currentState())
    }

    /** Call in onPause() with the same instance passed to addListener. */
    fun removeListener(listener: (Telemetry) -> Unit) {
        listeners.remove(listener)
    }

    private fun currentState() = Telemetry(
        temperature, humidity, pressure, batteryVoltage,
        fallDetected, sosActive, motionState, latitude, longitude, isOnline
    )

    private fun start() {
        if (started) return
        started = true

        val ref = FirebaseDatabase.getInstance().getReference(WORKER_PATH)
        Log.d(TAG, "Attaching listener to: $ref")

        // If nothing arrives at all, this still flips us to OFFLINE after the timeout
        // instead of leaving the UI in an indefinite "connecting" state.
        handler.postDelayed(offlineWatchdog, OFFLINE_TIMEOUT_MS)

        ref.addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                if (!snapshot.exists()) {
                    Log.w(TAG, "onDataChange: node \"$WORKER_PATH\" does not exist / is empty")
                }

                val env = snapshot.child("environment")
                temperature = env.child("temperature").getValue(Double::class.java)
                humidity = env.child("humidity").getValue(Double::class.java)
                pressure = env.child("pressure").getValue(Double::class.java)

                batteryVoltage = snapshot.child("battery").child("voltage").getValue(Double::class.java)

                val alerts = snapshot.child("alerts")
                fallDetected = alerts.child("fall_detected").getValue(Boolean::class.java)
                sosActive = alerts.child("sos_active").getValue(Boolean::class.java)

                motionState = snapshot.child("status").child("motion_state").getValue(String::class.java)

                val gps = snapshot.child("gps")
                latitude = gps.child("latitude").getValue(Double::class.java)
                longitude = gps.child("longitude").getValue(Double::class.java)

                isOnline = true

                Log.d(TAG, "onDataChange: temp=$temperature humidity=$humidity pressure=$pressure " +
                    "battery=$batteryVoltage fall=$fallDetected sos=$sosActive motion=$motionState")

                // Reset the offline countdown every time fresh data arrives.
                handler.removeCallbacks(offlineWatchdog)
                handler.postDelayed(offlineWatchdog, OFFLINE_TIMEOUT_MS)

                logHistoryPoint()
                notifyListeners()
            }

            override fun onCancelled(error: DatabaseError) {
                // This fires when the Realtime Database security RULES reject the read
                // (most common cause of "Firebase updates but app doesn't"). Previously
                // this error was silently swallowed — now it's logged so it's visible.
                Log.e(TAG, "Firebase read cancelled/denied: [${error.code}] ${error.message}", error.toException())
                isOnline = false
                notifyListeners()
            }
        })
    }

    private fun notifyListeners() {
        val state = currentState()
        listeners.forEach { it(state) }
    }

    /** Appends the current reading to HISTORY_PATH, at most once per HISTORY_LOG_INTERVAL_MS. */
    private fun logHistoryPoint() {
        val now = System.currentTimeMillis()
        if (now - lastHistoryLogAt < HISTORY_LOG_INTERVAL_MS) return
        lastHistoryLogAt = now

        val point = mapOf(
            "timestamp" to now,
            "temperature" to temperature,
            "humidity" to humidity,
            "pressure" to pressure,
            "motion_state" to motionState,
            "fall_detected" to fallDetected,
            "sos_active" to sosActive
        )

        FirebaseDatabase.getInstance().getReference(HISTORY_PATH).push().setValue(point)
            .addOnFailureListener { e ->
                Log.e(TAG, "Failed to log history point (check Realtime Database write rules)", e)
            }
    }

    /**
     * Reads every history point logged between [startMillis] and now (inclusive),
     * for the Report screen's charts and Excel export. Calls back on the main
     * thread with the records sorted oldest -> newest, or an empty list if
     * there's no history yet or the read fails (e.g. rules deny it).
     */
    fun fetchHistory(startMillis: Long, callback: (List<HistoryRecord>) -> Unit) {
        val ref = FirebaseDatabase.getInstance().getReference(HISTORY_PATH)
        ref.orderByChild("timestamp").startAt(startMillis.toDouble())
            .addListenerForSingleValueEvent(object : ValueEventListener {
                override fun onDataChange(snapshot: DataSnapshot) {
                    val records = snapshot.children.mapNotNull { child ->
                        val ts = child.child("timestamp").getValue(Long::class.java) ?: return@mapNotNull null
                        HistoryRecord(
                            timestampMillis = ts,
                            temperature = child.child("temperature").getValue(Double::class.java),
                            humidity = child.child("humidity").getValue(Double::class.java),
                            pressure = child.child("pressure").getValue(Double::class.java),
                            motionState = child.child("motion_state").getValue(String::class.java),
                            fallDetected = child.child("fall_detected").getValue(Boolean::class.java),
                            sosActive = child.child("sos_active").getValue(Boolean::class.java)
                        )
                    }.sortedBy { it.timestampMillis }
                    callback(records)
                }

                override fun onCancelled(error: DatabaseError) {
                    Log.e(TAG, "fetchHistory cancelled/denied: [${error.code}] ${error.message}", error.toException())
                    callback(emptyList())
                }
            })
    }
}
