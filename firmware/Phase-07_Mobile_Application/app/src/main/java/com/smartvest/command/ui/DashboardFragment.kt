package com.smartvest.command.ui

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.smartvest.command.R
import com.smartvest.command.data.AlertAdapter
import com.smartvest.command.data.EventHistoryAdapter
import com.smartvest.command.data.EventHistoryItem
import com.smartvest.command.databinding.FragmentDashboardBinding
import com.smartvest.command.util.AlertEvaluator
import com.smartvest.command.util.PrefsManager
import com.smartvest.command.util.ThemeManager
import com.smartvest.command.util.VestTelemetryManager
import java.text.SimpleDateFormat
import java.util.Locale

class DashboardFragment : Fragment() {

    companion object {
        private const val EVENT_HISTORY_ROW_LIMIT = 10
        private const val EVENT_HISTORY_LOG_INTERVAL_MS = 1_000L
    }

    private var _binding: FragmentDashboardBinding? = null
    private val binding get() = _binding!!

    // Rolling, device-local log (not read from Firebase) of the last
    // EVENT_HISTORY_ROW_LIMIT snapshots, newest first. Each snapshot uses the
    // device's own clock (System.currentTimeMillis()), not a server timestamp.
    private val eventHistory = ArrayDeque<EventHistoryItem>()
    private var latestTelemetry: VestTelemetryManager.Telemetry? = null
    private var wasOnline = true // assume online until proven otherwise, so a genuine offline start still logs once

    private val historyHandler = Handler(Looper.getMainLooper())
    private val historyLogger = object : Runnable {
        override fun run() {
            logEventHistorySnapshot()
            historyHandler.postDelayed(this, EVENT_HISTORY_LOG_INTERVAL_MS)
        }
    }

    private val telemetryListener: (VestTelemetryManager.Telemetry) -> Unit = { telemetry ->
        latestTelemetry = telemetry
        if (isAdded) updateLiveValues(telemetry)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentDashboardBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        ThemeManager.applyGlassCards(
            binding.cardWorkerStatus,
            binding.cardTemperature.root,
            binding.cardHumidity.root,
            binding.cardPressure.root,
            binding.cardActiveAlerts.root,
            binding.cardMotionState.root,
            binding.cardDashboardAlerts,
            binding.cardEventHistory
        )
        ThemeManager.applyTextMuted(
            binding.lblId, binding.lblName, binding.lblVestId, binding.lblStatus, binding.lblShift,
            binding.cardTemperature.txtSensorLabel, binding.cardHumidity.txtSensorLabel,
            binding.cardPressure.txtSensorLabel, binding.cardActiveAlerts.txtSensorLabel,
            binding.cardMotionState.txtSensorLabel,
            binding.txtDashboardNoAlerts,
            binding.txtNoHistory
        )
        ThemeManager.applyTextMain(
            binding.txtWorkerId, binding.txtWorkerName, binding.txtVestId, binding.txtShift,
            binding.cardTemperature.txtSensorValue, binding.cardHumidity.txtSensorValue,
            binding.cardPressure.txtSensorValue, binding.cardActiveAlerts.txtSensorValue,
            binding.cardMotionState.txtSensorValue
        )
        ThemeManager.applyHeading(
            binding.txtWorkerStatusHeading, binding.txtDashboardAlertsHeading,
            binding.txtEventHistoryHeading, binding.txtColTime, binding.txtColEvent,
            binding.txtColStatus, binding.txtColMotionTemp
        )

        // Static sensor card labels & icons (values update live below)
        binding.cardTemperature.txtSensorLabel.text = getString(R.string.temperature)
        binding.cardTemperature.imgSensorIcon.setImageResource(R.drawable.ic_thermometer)

        binding.cardHumidity.txtSensorLabel.text = getString(R.string.humidity)
        binding.cardHumidity.imgSensorIcon.setImageResource(R.drawable.ic_humidity)

        binding.cardPressure.txtSensorLabel.text = getString(R.string.pressure)
        binding.cardPressure.imgSensorIcon.setImageResource(R.drawable.ic_pressure)

        binding.cardActiveAlerts.txtSensorLabel.text = getString(R.string.active_alerts)
        binding.cardActiveAlerts.imgSensorIcon.setImageResource(R.drawable.ic_alert)

        binding.cardMotionState.txtSensorLabel.text = getString(R.string.motion_state)
        binding.cardMotionState.imgSensorIcon.setImageResource(R.drawable.ic_motion)
        binding.cardMotionState.txtSensorValue.textSize = 15f

        // Neon per-sensor colors (glow border + icon circle), theme-aware so they
        // switch correctly between dark/light like every other card.
        val isDark = PrefsManager.isDarkTheme

        binding.cardHumidity.root.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_cyan_dark else R.drawable.bg_glow_cyan_light
        )
        binding.cardHumidity.frameSensorIcon.setBackgroundResource(
            if (isDark) R.drawable.bg_icon_circle else R.drawable.bg_icon_circle_cyan_deep
        )
        binding.cardHumidity.imgSensorIcon.imageTintList = android.content.res.ColorStateList.valueOf(
            requireContext().getColor(if (isDark) R.color.accent_cyan else R.color.accent_cyan_deep)
        )

        binding.cardMotionState.root.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_amber_dark else R.drawable.bg_glow_amber_light
        )
        binding.cardMotionState.frameSensorIcon.setBackgroundResource(R.drawable.bg_icon_circle_amber)
        binding.cardMotionState.imgSensorIcon.imageTintList =
            android.content.res.ColorStateList.valueOf(requireContext().getColor(R.color.accent_amber))

        binding.cardTemperature.root.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_red_dark else R.drawable.bg_glow_red_light
        )
        binding.cardTemperature.frameSensorIcon.setBackgroundResource(R.drawable.bg_icon_circle_red)
        binding.cardTemperature.imgSensorIcon.imageTintList =
            android.content.res.ColorStateList.valueOf(requireContext().getColor(R.color.accent_red))

        binding.cardPressure.root.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_purple_dark else R.drawable.bg_glow_purple_light
        )
        binding.cardPressure.frameSensorIcon.setBackgroundResource(R.drawable.bg_icon_circle_purple)
        binding.cardPressure.imgSensorIcon.imageTintList =
            android.content.res.ColorStateList.valueOf(requireContext().getColor(R.color.accent_purple))

        // Active Alerts sensor card: 2-layer neon red, red bell icon
        binding.cardActiveAlerts.root.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_red_dark else R.drawable.bg_glow_red_light
        )
        binding.cardActiveAlerts.frameSensorIcon.setBackgroundResource(R.drawable.bg_icon_circle_red)
        binding.cardActiveAlerts.imgSensorIcon.imageTintList =
            android.content.res.ColorStateList.valueOf(requireContext().getColor(R.color.accent_red))

        // Worker Status card: 2-layer neon, green by default (matches the
        // default "Active" text/color below until live telemetry says otherwise)
        binding.cardWorkerStatus.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_green_dark else R.drawable.bg_glow_green_light
        )

        // Active alerts list at the bottom of the dashboard
        binding.recyclerDashboardAlerts.layoutManager = LinearLayoutManager(requireContext())
        binding.recyclerDashboardAlerts.adapter = AlertAdapter(emptyList())

        // Event / Alert History table (very bottom of the dashboard)
        binding.recyclerEventHistory.layoutManager = LinearLayoutManager(requireContext())
        binding.recyclerEventHistory.adapter = EventHistoryAdapter(emptyList())
        renderEventHistory()
    }

    /**
     * Driven by VestTelemetryManager (Firebase Realtime Database). Shows "--" instead
     * of any fake/default value whenever the vest is offline (no update for 6s).
     */
    private fun updateLiveValues(telemetry: VestTelemetryManager.Telemetry) {
        binding.txtWorkerStatusValue.text =
            getString(if (telemetry.isOnline) R.string.status_active else R.string.status_inactive)
        binding.txtWorkerStatusValue.setTextColor(
            requireContext().getColor(if (telemetry.isOnline) R.color.accent_green else R.color.accent_red)
        )

        // Worker Status card neon: 2-layer green when active, 2-layer yellow/amber when inactive
        val isDark = PrefsManager.isDarkTheme
        binding.cardWorkerStatus.setBackgroundResource(
            if (telemetry.isOnline) {
                if (isDark) R.drawable.bg_glow_green_dark else R.drawable.bg_glow_green_light
            } else {
                if (isDark) R.drawable.bg_glow_amber_dark else R.drawable.bg_glow_amber_light
            }
        )

        if (!telemetry.isOnline) {
            binding.cardTemperature.txtSensorValue.text = "--"
            binding.cardHumidity.txtSensorValue.text = "--"
            binding.cardPressure.txtSensorValue.text = "--"
            binding.cardActiveAlerts.txtSensorValue.text = "--"
            binding.cardMotionState.txtSensorValue.text = "--"
            updateDashboardAlertsList(emptyList())
            return
        }

        binding.cardTemperature.txtSensorValue.text =
            telemetry.temperature?.let { String.format(java.util.Locale.US, "%.1f°C", it) } ?: "--"
        binding.cardHumidity.txtSensorValue.text =
            telemetry.humidity?.let { String.format(java.util.Locale.US, "%.1f%%", it) } ?: "--"
        binding.cardPressure.txtSensorValue.text =
            telemetry.pressure?.let { String.format(java.util.Locale.US, "%.1f hPa", it) } ?: "--"

        // Real alert count from Firebase (alerts/fall_detected, alerts/sos_active)
        // instead of a hardcoded placeholder.
        val activeAlerts = listOf(telemetry.fallDetected, telemetry.sosActive).count { it == true }
        binding.cardActiveAlerts.txtSensorValue.text = activeAlerts.toString()

        binding.cardMotionState.txtSensorValue.text =
            telemetry.motionState?.trim()?.takeIf { it.isNotEmpty() }?.uppercase() ?: "--"

        // Same hazard rules (AlertEvaluator) as the Alerts tab, so the dashboard's
        // list at the bottom always matches what's shown there.
        updateDashboardAlertsList(AlertEvaluator.evaluate(telemetry))
    }

    private fun updateDashboardAlertsList(alerts: List<com.smartvest.command.data.AlertItem>) {
        if (_binding == null) return
        binding.recyclerDashboardAlerts.adapter = AlertAdapter(alerts)
        binding.txtDashboardAlertCount.text = alerts.size.toString()
        binding.txtDashboardNoAlerts.visibility = if (alerts.isEmpty()) View.VISIBLE else View.GONE
        binding.recyclerDashboardAlerts.visibility = if (alerts.isEmpty()) View.GONE else View.VISIBLE
    }

    /**
     * Appends one snapshot to the device-local Event / Alert History log, using the
     * device's own clock and whatever telemetry is currently known (from the live
     * Firebase listener) — this log itself is never read from or written to
     * Firebase, and only lives in memory on this device. Called every
     * EVENT_HISTORY_LOG_INTERVAL_MS while the Dashboard is visible.
     *
     * While offline, this logs exactly ONE "Connection Lost" / OFFLINE row at the
     * moment it happens (not every tick, so the table doesn't fill up with
     * duplicate offline rows) and then stays quiet until the vest comes back online.
     */
    private fun logEventHistorySnapshot() {
        if (!isAdded || _binding == null) return
        val telemetry = latestTelemetry ?: return

        if (!telemetry.isOnline) {
            if (wasOnline) {
                wasOnline = false
                addHistoryRow(
                    EventHistoryItem(
                        time = currentTimeText(),
                        eventName = getString(R.string.event_connection_lost),
                        isSafe = false,
                        motionTempText = getString(R.string.no_data_received),
                        isOffline = true
                    )
                )
            }
            return
        }
        wasOnline = true

        val isSafe = AlertEvaluator.evaluate(telemetry).isEmpty()
        val motionText = telemetry.motionState?.trim()?.takeIf { it.isNotEmpty() }?.uppercase(Locale.US) ?: "--"
        val tempText = telemetry.temperature?.let { String.format(Locale.US, "%.1f°C", it) } ?: "--"

        addHistoryRow(
            EventHistoryItem(
                time = currentTimeText(),
                eventName = getString(if (isSafe) R.string.event_sync else R.string.event_alert),
                isSafe = isSafe,
                motionTempText = "$motionText | $tempText"
            )
        )
    }

    private fun currentTimeText(): String =
        SimpleDateFormat("HH:mm:ss", Locale.US).format(java.util.Date(System.currentTimeMillis()))

    private fun addHistoryRow(item: EventHistoryItem) {
        eventHistory.addFirst(item)
        while (eventHistory.size > EVENT_HISTORY_ROW_LIMIT) {
            eventHistory.removeLast()
        }
        renderEventHistory()
    }

    private fun renderEventHistory() {
        if (_binding == null) return
        val rows = eventHistory.toList()
        binding.recyclerEventHistory.adapter = EventHistoryAdapter(rows)
        binding.txtNoHistory.visibility = if (rows.isEmpty()) View.VISIBLE else View.GONE
        binding.recyclerEventHistory.visibility = if (rows.isEmpty()) View.GONE else View.VISIBLE
    }

    override fun onResume() {
        super.onResume()
        VestTelemetryManager.addListener(telemetryListener)
        historyHandler.removeCallbacks(historyLogger)
        historyHandler.post(historyLogger)
    }

    override fun onPause() {
        super.onPause()
        VestTelemetryManager.removeListener(telemetryListener)
        historyHandler.removeCallbacks(historyLogger)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        historyHandler.removeCallbacks(historyLogger)
        _binding = null
    }
}
