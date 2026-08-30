package com.smartvest.command.ui

import android.graphics.Color
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.github.mikephil.charting.charts.LineChart
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.Entry
import com.github.mikephil.charting.data.LineData
import com.github.mikephil.charting.data.LineDataSet
import com.smartvest.command.R
import com.smartvest.command.databinding.FragmentLiveChartBinding
import com.smartvest.command.util.PrefsManager
import com.smartvest.command.util.ThemeManager
import com.smartvest.command.util.VestTelemetryManager

class LiveChartFragment : Fragment() {

    private var _binding: FragmentLiveChartBinding? = null
    private val binding get() = _binding!!

    private var timeIndex = 0f

    private lateinit var tempSet: LineDataSet
    private lateinit var humiditySet: LineDataSet
    private lateinit var pressureSet: LineDataSet

    private val telemetryListener: (VestTelemetryManager.Telemetry) -> Unit = { telemetry ->
        if (isAdded) onTelemetry(telemetry)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentLiveChartBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        ThemeManager.applyGlassCards(
            binding.cardTempChart, binding.cardHumidityChart, binding.cardPressureChart
        )
        ThemeManager.applyHeading(
            binding.txtLiveTempChartHeading, binding.txtLiveHumidityChartHeading, binding.txtLivePressureChartHeading
        )

        // Match the Dashboard's per-sensor neon: Temperature = red, Humidity = cyan, Pressure = purple
        val isDark = PrefsManager.isDarkTheme
        binding.cardTempChart.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_red_dark else R.drawable.bg_glow_red_light
        )
        binding.cardHumidityChart.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_cyan_dark else R.drawable.bg_glow_cyan_light
        )
        binding.cardPressureChart.setBackgroundResource(
            if (isDark) R.drawable.bg_glow_purple_dark else R.drawable.bg_glow_purple_light
        )

        val red = ContextCompat.getColor(requireContext(), R.color.accent_red)
        val cyan = ContextCompat.getColor(requireContext(), R.color.accent_cyan)
        val purple = ContextCompat.getColor(requireContext(), R.color.accent_purple)

        tempSet = buildDataSet("Temperature", red)
        humiditySet = buildDataSet("Humidity", cyan)
        pressureSet = buildDataSet("Pressure", purple)

        styleChart(binding.chartTemperature, tempSet)
        styleChart(binding.chartHumidity, humiditySet)
        styleChart(binding.chartPressure, pressureSet)
    }

    private fun buildDataSet(label: String, color: Int): LineDataSet {
        val set = LineDataSet(mutableListOf(), label)
        set.color = color
        set.setDrawCircles(false)
        set.lineWidth = 2f
        set.mode = LineDataSet.Mode.CUBIC_BEZIER
        set.setDrawFilled(true)
        set.fillColor = color
        set.fillAlpha = 40
        set.setDrawValues(false)
        set.highLightColor = color
        return set
    }

    private fun styleChart(chart: LineChart, dataSet: LineDataSet) {
        val labelColor = if (PrefsManager.isDarkTheme) Color.parseColor("#8493A8") else Color.BLACK
        chart.data = LineData(dataSet)
        chart.description.isEnabled = false
        chart.legend.isEnabled = false
        chart.setTouchEnabled(false)
        chart.setBackgroundColor(Color.TRANSPARENT)
        chart.axisRight.isEnabled = false
        chart.axisLeft.textColor = labelColor
        chart.axisLeft.gridColor = Color.parseColor("#1AFFFFFF")
        chart.xAxis.position = XAxis.XAxisPosition.BOTTOM
        chart.xAxis.textColor = labelColor
        chart.xAxis.gridColor = Color.parseColor("#1AFFFFFF")
        chart.setNoDataText("Waiting for telemetry…")
        chart.setNoDataTextColor(Color.parseColor("#8493A8"))
        chart.invalidate()
    }

    /**
     * Driven by VestTelemetryManager (Firebase Realtime Database). While the vest
     * is offline (no update for 6s) no new points are appended — the chart simply
     * holds its last real reading instead of drawing any fake/default data.
     */
    private fun onTelemetry(telemetry: VestTelemetryManager.Telemetry) {
        if (!telemetry.isOnline) return

        timeIndex += 1f
        telemetry.temperature?.let { pushEntry(binding.chartTemperature, tempSet, Entry(timeIndex, it.toFloat())) }
        telemetry.humidity?.let { pushEntry(binding.chartHumidity, humiditySet, Entry(timeIndex, it.toFloat())) }
        telemetry.pressure?.let { pushEntry(binding.chartPressure, pressureSet, Entry(timeIndex, it.toFloat())) }
    }

    private fun pushEntry(chart: LineChart, set: LineDataSet, entry: Entry) {
        set.addEntry(entry)
        if (set.entryCount > 30) {
            set.removeFirst()
        }
        chart.data?.notifyDataChanged()
        chart.notifyDataSetChanged()
        chart.setVisibleXRangeMaximum(30f)
        chart.moveViewToX(set.entryCount.toFloat())
    }

    override fun onResume() {
        super.onResume()
        VestTelemetryManager.addListener(telemetryListener)
    }

    override fun onPause() {
        super.onPause()
        VestTelemetryManager.removeListener(telemetryListener)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
