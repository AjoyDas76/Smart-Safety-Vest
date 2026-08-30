package com.smartvest.command.util

import android.content.Context
import android.content.SharedPreferences

/**
 * Central place for all persisted user preferences:
 * theme (light/dark), audio on/off, notification on/off, login session.
 */
object PrefsManager {

    private const val PREFS_NAME = "smart_vest_prefs"
    private const val KEY_DARK_THEME = "key_dark_theme"
    private const val KEY_AUDIO = "key_audio"
    private const val KEY_NOTIFICATION = "key_notification"
    private const val KEY_LOGGED_IN = "key_logged_in"

    private lateinit var prefs: SharedPreferences

    fun init(context: Context) {
        prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }

    var isDarkTheme: Boolean
        get() = prefs.getBoolean(KEY_DARK_THEME, true) // dark is default, matches the site
        set(value) = prefs.edit().putBoolean(KEY_DARK_THEME, value).apply()

    var isAudioOn: Boolean
        get() = prefs.getBoolean(KEY_AUDIO, true)
        set(value) = prefs.edit().putBoolean(KEY_AUDIO, value).apply()

    var isNotificationOn: Boolean
        get() = prefs.getBoolean(KEY_NOTIFICATION, true)
        set(value) = prefs.edit().putBoolean(KEY_NOTIFICATION, value).apply()

    var isLoggedIn: Boolean
        get() = prefs.getBoolean(KEY_LOGGED_IN, false)
        set(value) = prefs.edit().putBoolean(KEY_LOGGED_IN, value).apply()

    fun logout() {
        isLoggedIn = false
    }
}
