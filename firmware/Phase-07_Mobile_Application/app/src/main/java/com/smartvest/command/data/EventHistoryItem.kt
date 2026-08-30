package com.smartvest.command.data

/** One row of the Dashboard's "Event / Alert History" table. */
data class EventHistoryItem(
    val time: String,
    val eventName: String,
    val isSafe: Boolean,
    val motionTempText: String,
    val isOffline: Boolean = false
)
