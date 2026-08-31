package com.smartvest.command.util

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.smartvest.command.R
import com.smartvest.command.data.AlertItem
import com.smartvest.command.ui.MainActivity

/**
 * Fires a real system notification whenever a NEW hazard alert appears
 * (temperature/humidity/pressure out of range, fall, SOS — see
 * AlertEvaluator). Previously the Settings > Notification switch only
 * saved a preference and nothing in the app ever actually called
 * NotificationManager, so no notification could appear regardless of the
 * switch or the system notification permission.
 */
object NotificationHelper {

    private const val CHANNEL_ID = "vest_hazard_alerts"
    private var nextNotificationId = 1000

    fun ensureChannel(context: Context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            if (manager.getNotificationChannel(CHANNEL_ID) == null) {
                val channel = NotificationChannel(
                    CHANNEL_ID,
                    "Vest Hazard Alerts",
                    NotificationManager.IMPORTANCE_HIGH
                ).apply {
                    description = "Fall, SOS, and out-of-range sensor alerts from the safety vest"
                    enableVibration(true)
                }
                manager.createNotificationChannel(channel)
            }
        }
    }

    /**
     * Shows one notification for [alert]. No-op if the user turned
     * notifications off in Settings, or the system POST_NOTIFICATIONS
     * permission isn't granted (required on Android 13+).
     */
    fun show(context: Context, alert: AlertItem) {
        if (!PrefsManager.isNotificationOn) return

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS)
            != PackageManager.PERMISSION_GRANTED
        ) return

        ensureChannel(context)

        val openIntent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            context, 0, openIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_alert)
            .setContentTitle("⚠ ${alert.title}")
            .setContentText(alert.detail)
            .setStyle(NotificationCompat.BigTextStyle().bigText(alert.detail))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setCategory(NotificationCompat.CATEGORY_ALARM)
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)
            .build()

        try {
            NotificationManagerCompat.from(context).notify(nextNotificationId++, notification)
        } catch (e: SecurityException) {
            // Permission was revoked between the check above and this call — ignore.
        }
    }
}
