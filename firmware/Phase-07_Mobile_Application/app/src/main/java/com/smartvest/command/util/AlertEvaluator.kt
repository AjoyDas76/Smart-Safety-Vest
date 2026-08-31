package com.smartvest.command.util

import com.smartvest.command.R
import com.smartvest.command.data.AlertItem
import java.util.Locale

/**
 * Single source of truth for which hazard conditions count as an active
 * "alert", matching the web dashboard's Active Alerts rules exactly.
 * Used by both AlertsFragment (to list them) and the notification trigger
 * in MainActivity (to notify on new ones), so the two can never drift apart.
 */
object AlertEvaluator {

    fun evaluate(telemetry: VestTelemetryManager.Telemetry): List<AlertItem> {
        if (!telemetry.isOnline) return emptyList()

        val alerts = mutableListOf<AlertItem>()

        telemetry.temperature?.let { t ->
            if (t > 45 || t < 15) {
                alerts.add(
                    AlertItem(
                        title = if (t > 45) "High Temperature" else "Low Temperature",
                        detail = String.format(
                            Locale.US,
                            "Reading is %.1f°C — outside the safe range of 15°C – 45°C", t
                        ),
                        iconRes = R.drawable.ic_thermometer
                    )
                )
            }
        }

        telemetry.humidity?.let { h ->
            if (h > 90 || h < 20) {
                alerts.add(
                    AlertItem(
                        title = if (h > 90) "High Humidity" else "Low Humidity",
                        detail = String.format(
                            Locale.US,
                            "Reading is %.1f%% — outside the safe range of 20%% – 90%%", h
                        ),
                        iconRes = R.drawable.ic_humidity
                    )
                )
            }
        }

        telemetry.pressure?.let { p ->
            if (p > 1050 || p < 950) {
                alerts.add(
                    AlertItem(
                        title = if (p > 1050) "High Pressure" else "Low Pressure",
                        detail = String.format(
                            Locale.US,
                            "Reading is %.0f hPa — outside the safe range of 950 – 1050 hPa", p
                        ),
                        iconRes = R.drawable.ic_pressure
                    )
                )
            }
        }

        if (telemetry.motionState?.trim()?.uppercase(Locale.US) == "FALL") {
            alerts.add(
                AlertItem(
                    title = "Fall Detected (Motion)",
                    detail = "The motion sensor reports the worker is in a FALL state",
                    iconRes = R.drawable.ic_motion
                )
            )
        }

        if (telemetry.fallDetected == true) {
            alerts.add(
                AlertItem(
                    title = "Fall Detected",
                    detail = "The vest's fall-detection alert flag is active",
                    iconRes = R.drawable.ic_alert
                )
            )
        }

        if (telemetry.sosActive == true) {
            alerts.add(
                AlertItem(
                    title = "SOS Active",
                    detail = "The worker has triggered the emergency SOS button",
                    iconRes = R.drawable.ic_alert
                )
            )
        }

        return alerts
    }
}
