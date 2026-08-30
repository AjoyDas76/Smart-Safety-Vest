package com.smartvest.command.data

import androidx.annotation.DrawableRes
import com.smartvest.command.R

data class AlertItem(
    val title: String,
    val detail: String,
    @DrawableRes val iconRes: Int = R.drawable.ic_alert
)
