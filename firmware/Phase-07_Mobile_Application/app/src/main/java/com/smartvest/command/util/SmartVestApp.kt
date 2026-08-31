package com.smartvest.command.util

import android.app.Application

class SmartVestApp : Application() {
    override fun onCreate() {
        super.onCreate()
        PrefsManager.init(this)
    }
}
