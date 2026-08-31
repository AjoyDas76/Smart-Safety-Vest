package com.smartvest.command.ui

import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.content.FileProvider
import androidx.fragment.app.Fragment
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.BarData
import com.github.mikephil.charting.data.BarDataSet
import com.github.mikephil.charting.data.BarEntry
import com.github.mikephil.charting.formatter.IndexAxisValueFormatter
import com.google.android.material.tabs.TabLayout
import com.smartvest.command.R
import com.smartvest.command.databinding.FragmentReportBinding
import com.smartvest.command.util.ExcelExporter
import com.smartvest.command.util.PrefsManager
import com.smartvest.command.util.ThemeManager
import com.smartvest.command.util.VestTelemetryManager
import java.io.File
import java.text.SimpleDateFormat
import java.util.Locale

class ReportFragment : Fragment() {

    private var _binding: FragmentReportBinding? = null
    private val binding get() = _binding!!

    // Same worker identity shown (statically) on the Dashboard screen — there is
    // no separate Firebase "profile" node, so the export uses the same values
    // already displayed elsewhere in the app.
    private val workerId = "VST-001"
    private val workerName = "Rahim Uddin"
    private val batchId = "SV-2026-014"

    private var currentPeriod = ReportPeriod.DAILY

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentReportBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        ThemeManager.applyGlassCards(binding.cardReport)
        ThemeManager.applyTextMuted(binding.txtReportSummary)
        ThemeManager.applyHeading(binding.txtReportHeading)

        // Daily / Weekly / Monthly tab labels: black in light theme (the
        // default cyan/muted pair is too low-contrast on the light glass
        // background), unchanged in dark theme.
        if (!PrefsManager.isDarkTheme) {
            val black = requireContext().getColor(R.color.black)
            binding.tabReport.setTabTextColors(black, black)
        }

        renderReport(ReportPeriod.DAILY)

        binding.tabReport.addOnTabSelectedListener(object : TabLayout.OnTabSelectedListener {
            override fun onTabSelected(tab: TabLayout.Tab) {
                val period = when (tab.position) {
                    0 -> ReportPeriod.DAILY
                    1 -> ReportPeriod.WEEKLY
                    else -> ReportPeriod.MONTHLY
                }
                renderReport(period)
            }
            override fun onTabUnselected(tab: TabLayout.Tab) {}
            override fun onTabReselected(tab: TabLayout.Tab) {}
        })

        binding.btnExportDaily.setOnClickListener { exportPeriod(ReportPeriod.DAILY) }
        binding.btnExportWeekly.setOnClickListener { exportPeriod(ReportPeriod.WEEKLY) }
        binding.btnExportMonthly.setOnClickListener { exportPeriod(ReportPeriod.MONTHLY) }
    }

    private enum class ReportPeriod { DAILY, WEEKLY, MONTHLY }

    /** How far back each period looks, in milliseconds. */
    private fun periodRangeMillis(period: ReportPeriod): Long = when (period) {
        ReportPeriod.DAILY -> 24L * 60 * 60 * 1000
        ReportPeriod.WEEKLY -> 7L * 24 * 60 * 60 * 1000
        ReportPeriod.MONTHLY -> 30L * 24 * 60 * 60 * 1000
    }

    /**
     * Pulled from Firebase (worker1/history — logged continuously by
     * VestTelemetryManager). Chart + summary show real data whenever history
     * exists for the selected period, and a plain "no data yet" message
     * otherwise, instead of any fake/default numbers.
     */
    private fun renderReport(period: ReportPeriod) {
        currentPeriod = period

        val labels = when (period) {
            ReportPeriod.DAILY -> listOf("6am", "9am", "12pm", "3pm", "6pm", "9pm")
            ReportPeriod.WEEKLY -> listOf("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")
            ReportPeriod.MONTHLY -> listOf("W1", "W2", "W3", "W4")
        }

        val now = System.currentTimeMillis()
        val startMillis = now - periodRangeMillis(period)

        VestTelemetryManager.fetchHistory(startMillis) { records ->
            if (!isAdded || currentPeriod != period) return@fetchHistory

            val entries = mutableListOf<BarEntry>()
            if (records.isNotEmpty()) {
                val bucketWidth = (now - startMillis).toFloat() / labels.size
                val bucketSums = FloatArray(labels.size)
                val bucketCounts = IntArray(labels.size)

                records.forEach { record ->
                    val temp = record.temperature ?: return@forEach
                    val bucket = (((record.timestampMillis - startMillis) / bucketWidth).toInt())
                        .coerceIn(0, labels.size - 1)
                    bucketSums[bucket] += temp.toFloat()
                    bucketCounts[bucket] += 1
                }

                for (i in labels.indices) {
                    if (bucketCounts[i] > 0) {
                        entries.add(BarEntry(i.toFloat(), bucketSums[i] / bucketCounts[i]))
                    }
                }
            }

            val dataSet = BarDataSet(entries, "Avg Temperature (°C)")
            dataSet.color = Color.parseColor("#00F2FF")
            dataSet.setDrawValues(false)

            val labelColor = if (PrefsManager.isDarkTheme) Color.parseColor("#8493A8") else Color.BLACK

            binding.chartReport.apply {
                data = BarData(dataSet)
                description.isEnabled = false
                legend.isEnabled = false
                setTouchEnabled(false)
                setBackgroundColor(Color.TRANSPARENT)
                axisRight.isEnabled = false
                axisLeft.textColor = labelColor
                axisLeft.gridColor = Color.parseColor("#1AFFFFFF")
                xAxis.position = XAxis.XAxisPosition.BOTTOM
                xAxis.granularity = 1f
                xAxis.textColor = labelColor
                xAxis.gridColor = Color.TRANSPARENT
                xAxis.valueFormatter = IndexAxisValueFormatter(labels)
                invalidate()
            }

            binding.txtReportSummary.text = buildSummaryText(records)
        }
    }

    private fun buildSummaryText(records: List<VestTelemetryManager.HistoryRecord>): String {
        if (records.isEmpty()) return getString(R.string.report_no_data)

        val temps = records.mapNotNull { it.temperature }
        val humidities = records.mapNotNull { it.humidity }
        val pressures = records.mapNotNull { it.pressure }
        val alertCount = records.count { it.fallDetected == true || it.sosActive == true }

        val avgTemp = temps.takeIf { it.isNotEmpty() }?.average()
        val avgHumidity = humidities.takeIf { it.isNotEmpty() }?.average()
        val avgPressure = pressures.takeIf { it.isNotEmpty() }?.average()

        return buildString {
            append("${records.size} readings logged.")
            if (avgTemp != null) append(String.format(Locale.US, " Avg temp %.1f°C.", avgTemp))
            if (avgHumidity != null) append(String.format(Locale.US, " Avg humidity %.1f%%.", avgHumidity))
            if (avgPressure != null) append(String.format(Locale.US, " Avg pressure %.0f hPa.", avgPressure))
            append(" Alerts in period: $alertCount.")
        }
    }

    /**
     * Fetches the raw (un-bucketed) history for [period] and writes it out as an
     * .xlsx file: worker ID/name/batch ID up top, then one row per logged
     * reading with temperature, humidity, pressure, motion state, and alerts
     * each in their own column — then opens the system share/"open with" sheet
     * so it can be saved, emailed, or opened in a spreadsheet app.
     */
    private fun exportPeriod(period: ReportPeriod) {
        Toast.makeText(requireContext(), R.string.report_exporting, Toast.LENGTH_SHORT).show()

        val now = System.currentTimeMillis()
        val startMillis = now - periodRangeMillis(period)

        VestTelemetryManager.fetchHistory(startMillis) { records ->
            if (!isAdded) return@fetchHistory

            if (records.isEmpty()) {
                Toast.makeText(requireContext(), R.string.report_export_empty, Toast.LENGTH_SHORT).show()
                return@fetchHistory
            }

            try {
                val file = buildExcelFile(period, records)
                shareFile(file)
            } catch (e: Exception) {
                Toast.makeText(requireContext(), R.string.report_export_failed, Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun buildExcelFile(
        period: ReportPeriod,
        records: List<VestTelemetryManager.HistoryRecord>
    ): File {
        val periodLabel = when (period) {
            ReportPeriod.DAILY -> "Daily"
            ReportPeriod.WEEKLY -> "Weekly"
            ReportPeriod.MONTHLY -> "Monthly"
        }

        val timestampFormat = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)

        val rows = mutableListOf<List<String>>()
        rows.add(listOf("Worker ID", workerId))
        rows.add(listOf("Name", workerName))
        rows.add(listOf("Batch ID", batchId))
        rows.add(listOf("Report Period", periodLabel))
        rows.add(listOf("Generated", timestampFormat.format(java.util.Date())))
        rows.add(emptyList())
        rows.add(
            listOf(
                "Timestamp", "Temperature (°C)", "Humidity (%)", "Pressure (hPa)",
                "Motion State", "Fall Detected", "SOS Active"
            )
        )
        records.forEach { r ->
            rows.add(
                listOf(
                    timestampFormat.format(java.util.Date(r.timestampMillis)),
                    r.temperature?.toString() ?: "",
                    r.humidity?.toString() ?: "",
                    r.pressure?.toString() ?: "",
                    r.motionState ?: "",
                    if (r.fallDetected == true) "Yes" else "No",
                    if (r.sosActive == true) "Yes" else "No"
                )
            )
        }

        val fileStamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(java.util.Date())
        val outDir = File(requireContext().getExternalFilesDir(null), "reports")
        val outFile = File(outDir, "SmartVest_${periodLabel}_Report_$fileStamp.xlsx")

        ExcelExporter.writeXlsx(outFile, "$periodLabel Report", rows)
        return outFile
    }

    private fun shareFile(file: File) {
        val uri = FileProvider.getUriForFile(
            requireContext(), "com.smartvest.command.fileprovider", file
        )
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivity(Intent.createChooser(intent, file.name))
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
