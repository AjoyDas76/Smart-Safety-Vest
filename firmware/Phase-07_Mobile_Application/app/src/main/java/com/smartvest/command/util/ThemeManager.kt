package com.smartvest.command.util

import android.view.View
import android.widget.TextView
import com.smartvest.command.R

/**
 * Applies the correct glass drawable / text colors depending on the
 * user's chosen theme (PrefsManager.isDarkTheme). Call these helpers
 * from each fragment's onViewCreated after inflating its glass cards.
 */
object ThemeManager {

    fun glassCardBg(): Int =
        if (PrefsManager.isDarkTheme) R.drawable.bg_glass_dark else R.drawable.bg_glass_light

    fun glassStrongBg(): Int =
        if (PrefsManager.isDarkTheme) R.drawable.bg_glass_strong_dark else R.drawable.bg_glass_strong_light

    fun bgGradient(): Int =
        if (PrefsManager.isDarkTheme) R.drawable.bg_app_gradient else R.drawable.bg_app_gradient_light

    fun drawerBg(): Int =
        if (PrefsManager.isDarkTheme) R.drawable.bg_drawer_glass else R.drawable.bg_drawer_glass_light

    fun textMain(): Int =
        if (PrefsManager.isDarkTheme) R.color.dark_text_main else R.color.light_text_main

    fun textMuted(): Int =
        if (PrefsManager.isDarkTheme) R.color.dark_text_muted else R.color.light_text_muted

    /** Recursively applies the glass card background to every direct card container passed in. */
    fun applyGlassCards(vararg views: View) {
        val bg = glassCardBg()
        views.forEach { it.setBackgroundResource(bg) }
    }

    fun applyTextMain(vararg views: TextView) {
        val color = textMain()
        views.forEach { it.setTextColor(it.context.getColor(color)) }
    }

    fun applyTextMuted(vararg views: TextView) {
        val color = textMuted()
        views.forEach { it.setTextColor(it.context.getColor(color)) }
    }

    /**
     * Section-title / column-header labels (e.g. "LIVE TEMPERATURE CHART",
     * "WORKER REPORT", "EVENT / ALERT HISTORY", table column headers). These are
     * cyan-on-dark by design, but plain cyan is too low-contrast on the light
     * background, so in light theme they switch to bold black instead. Dark
     * theme keeps its original cyan look untouched.
     */
    fun applyHeading(vararg views: TextView) {
        val isDark = PrefsManager.isDarkTheme
        views.forEach {
            it.setTextColor(it.context.getColor(if (isDark) R.color.accent_cyan else R.color.black))
            it.setTypeface(it.typeface, if (isDark) android.graphics.Typeface.NORMAL else android.graphics.Typeface.BOLD)
        }
    }
}
