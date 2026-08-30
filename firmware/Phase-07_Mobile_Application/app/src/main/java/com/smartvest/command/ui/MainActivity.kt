package com.smartvest.command.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.smartvest.command.R
import com.smartvest.command.databinding.ActivityMainBinding
import com.smartvest.command.util.AlertEvaluator
import com.smartvest.command.util.AlertSoundPlayer
import com.smartvest.command.util.NotificationHelper
import com.smartvest.command.util.PrefsManager
import com.smartvest.command.util.ThemeManager
import com.smartvest.command.util.VestTelemetryManager

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    // Tracks which hazard alerts have already triggered a notification so the
    // same ongoing alert (e.g. a fall that stays active for 10s of readings)
    // doesn't spam a new notification on every Firebase update — only newly
    // appearing alerts fire one. Mirrors the web dashboard's dedupe-by-title.
    private var lastNotifiedAlertTitles = emptySet<String>()

    private val telemetryListener: (VestTelemetryManager.Telemetry) -> Unit = { telemetry ->
        setOnlineStatus(telemetry.isOnline)
        notifyNewHazardAlerts(telemetry)
    }

    private val notificationPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { /* no-op either way */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        // Apply chosen theme before inflating views
        setTheme(if (PrefsManager.isDarkTheme) R.style.Theme_SmartVest else R.style.Theme_SmartVest_Light)
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        applyThemeToShell()
        NotificationHelper.ensureChannel(this)
        requestNotificationPermissionIfNeeded()

        binding.btnMenu.setOnClickListener {
            binding.drawerLayout.openDrawer(Gravity.START)
        }

        binding.navView.setNavigationItemSelectedListener { item ->
            val fragment: Fragment = when (item.itemId) {
                R.id.nav_dashboard -> DashboardFragment()
                R.id.nav_live_chart -> LiveChartFragment()
                R.id.nav_map_tracking -> MapTrackingFragment()
                R.id.nav_alert -> AlertsFragment()
                R.id.nav_report -> ReportFragment()
                R.id.nav_settings -> SettingsFragment()
                else -> DashboardFragment()
            }
            showFragment(fragment, item.itemId == R.id.nav_map_tracking)
            binding.drawerLayout.closeDrawer(Gravity.START)
            true
        }

        if (savedInstanceState == null) {
            showFragment(DashboardFragment(), fullscreen = false)
        }
    }

    /**
     * Android 13+ (API 33) requires the user to explicitly grant
     * POST_NOTIFICATIONS at runtime — declaring it in the manifest alone is
     * not enough. Without this, notifications would silently never appear
     * on those devices even with the in-app Settings switch turned on.
     */
    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
            != PackageManager.PERMISSION_GRANTED
        ) {
            notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
        }
    }

    /** Fires a system notification for each hazard alert that just appeared, and
     *  keeps the siren playing for as long as at least one alert is active. */
    private fun notifyNewHazardAlerts(telemetry: VestTelemetryManager.Telemetry) {
        val currentAlerts = AlertEvaluator.evaluate(telemetry)
        val currentTitles = currentAlerts.map { it.title }.toSet()

        currentAlerts
            .filter { it.title !in lastNotifiedAlertTitles }
            .forEach { NotificationHelper.show(this, it) }

        lastNotifiedAlertTitles = currentTitles

        // Siren keeps wailing continuously while ANY alert is active, and stops
        // automatically the instant the worker's state goes back to normal
        // (e.g. motion state changes away from FALL) and currentAlerts is empty.
        if (currentAlerts.isNotEmpty()) {
            AlertSoundPlayer.startSiren()
        } else {
            AlertSoundPlayer.stopSiren()
        }
    }

    /** Applies the current theme (PrefsManager.isDarkTheme) to the persistent shell UI. */
    private fun applyThemeToShell() {
        binding.rootContentLayout.setBackgroundResource(ThemeManager.bgGradient())
        binding.vestCommandBar.setBackgroundResource(ThemeManager.glassStrongBg())
        binding.drawerContent.setBackgroundResource(ThemeManager.drawerBg())

        val textColor = getColor(ThemeManager.textMain())
        binding.txtDrawerBrand.setTextColor(textColor)
        binding.txtVestCommandBar.setTextColor(textColor)
        binding.navView.itemTextColor = android.content.res.ColorStateList.valueOf(textColor)
    }

    /**
     * The VEST COMMAND bar is always present, EXCEPT when Map Tracking
     * is shown full-screen with its own close (X) button.
     */
    private fun showFragment(fragment: Fragment, fullscreen: Boolean) {
        binding.vestCommandBar.visibility = if (fullscreen) android.view.View.GONE else android.view.View.VISIBLE
        supportFragmentManager.beginTransaction()
            .replace(R.id.fragmentContainer, fragment)
            .commit()
    }

    /** Called by MapTrackingFragment's close (X) button to return to the Dashboard. */
    fun closeFullscreenMap() {
        binding.vestCommandBar.visibility = android.view.View.VISIBLE
        supportFragmentManager.beginTransaction()
            .replace(R.id.fragmentContainer, DashboardFragment())
            .commit()
    }

    fun setOnlineStatus(online: Boolean) {
        binding.txtStatus.text = getString(if (online) R.string.status_online else R.string.status_offline)
        binding.txtStatus.setTextColor(
            getColor(if (online) R.color.accent_green else R.color.accent_red)
        )
        binding.statusPill.setBackgroundResource(
            if (online) R.drawable.bg_status_pill_online else R.drawable.bg_status_pill_offline
        )
    }

    override fun onResume() {
        super.onResume()
        VestTelemetryManager.addListener(telemetryListener)
    }

    override fun onPause() {
        super.onPause()
        VestTelemetryManager.removeListener(telemetryListener)
        AlertSoundPlayer.stopSiren()
    }

    override fun onBackPressed() {
        if (binding.drawerLayout.isDrawerOpen(Gravity.START)) {
            binding.drawerLayout.closeDrawer(Gravity.START)
        } else {
            super.onBackPressed()
        }
    }
}
