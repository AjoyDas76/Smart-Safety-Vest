package com.smartvest.command.util

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import kotlin.math.sin
import kotlin.math.PI

/**
 * Plays a continuous siren-style wail (frequency sweeping up and down, like
 * an emergency siren) for as long as the worker has at least one active
 * hazard alert (fall, SOS, out-of-range temperature/humidity/pressure — see
 * AlertEvaluator), and stops automatically the moment their state goes back
 * to normal. Call [startSiren] on every telemetry update while an alert is
 * active and [stopSiren] the moment it isn't (MainActivity does this) —
 * both are safe to call repeatedly/redundantly.
 *
 * The siren audio is synthesized on the fly with AudioTrack instead of a
 * bundled sound file, so no new res/raw asset is required.
 *
 * Settings has an "Audio" switch (PrefsManager.isAudioOn, "Alert sounds
 * on/off") that gates this.
 */
object AlertSoundPlayer {

    private const val SAMPLE_RATE = 44100
    private const val MIN_FREQ_HZ = 500.0
    private const val MAX_FREQ_HZ = 1200.0
    private const val SWEEP_PERIOD_SEC = 1.2 // one full low->high->low wail cycle

    @Volatile private var isPlaying = false
    private var sirenThread: Thread? = null

    /** No-op if already playing (so repeated calls per telemetry update don't restart it) or Audio is off. */
    @Synchronized
    fun startSiren() {
        if (!PrefsManager.isAudioOn) return
        if (isPlaying) return

        isPlaying = true
        sirenThread = Thread(::runSirenLoop, "AlertSirenThread").apply { start() }
    }

    /** Stops and releases the siren. No-op if it isn't playing. */
    @Synchronized
    fun stopSiren() {
        isPlaying = false
    }

    private fun runSirenLoop() {
        val minBufferSize = AudioTrack.getMinBufferSize(
            SAMPLE_RATE, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT
        )
        if (minBufferSize <= 0) {
            isPlaying = false
            return
        }

        var audioTrack: AudioTrack? = null
        try {
            audioTrack = AudioTrack(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_ALARM)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build(),
                AudioFormat.Builder()
                    .setSampleRate(SAMPLE_RATE)
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                    .build(),
                minBufferSize,
                AudioTrack.MODE_STREAM,
                AudioManager.AUDIO_SESSION_ID_GENERATE
            )
            audioTrack.play()

            val chunkSamples = 1024
            val buffer = ShortArray(chunkSamples)
            var phase = 0.0
            var t = 0.0

            while (isPlaying) {
                for (i in 0 until chunkSamples) {
                    // Triangle-wave sweep between MIN_FREQ_HZ and MAX_FREQ_HZ -> classic siren wail.
                    val cyclePos = (t % SWEEP_PERIOD_SEC) / SWEEP_PERIOD_SEC
                    val freq = if (cyclePos < 0.5) {
                        MIN_FREQ_HZ + (MAX_FREQ_HZ - MIN_FREQ_HZ) * (cyclePos / 0.5)
                    } else {
                        MAX_FREQ_HZ - (MAX_FREQ_HZ - MIN_FREQ_HZ) * ((cyclePos - 0.5) / 0.5)
                    }

                    phase += 2.0 * PI * freq / SAMPLE_RATE
                    if (phase > 2.0 * PI) phase -= 2.0 * PI

                    buffer[i] = (sin(phase) * Short.MAX_VALUE * 0.85).toInt().toShort()
                    t += 1.0 / SAMPLE_RATE
                }
                audioTrack.write(buffer, 0, chunkSamples)
            }
        } catch (e: Exception) {
            // No usable audio output (e.g. some emulators/devices) — the
            // system notification still appears, so fail silently.
        } finally {
            try {
                audioTrack?.stop()
            } catch (e: Exception) { /* already stopped */ }
            audioTrack?.release()
            isPlaying = false
        }
    }
}
