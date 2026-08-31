package com.smartvest.command.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.smartvest.command.data.AlertAdapter
import com.smartvest.command.databinding.FragmentAlertsBinding
import com.smartvest.command.util.AlertEvaluator
import com.smartvest.command.util.ThemeManager
import com.smartvest.command.util.VestTelemetryManager

class AlertsFragment : Fragment() {

    private var _binding: FragmentAlertsBinding? = null
    private val binding get() = _binding!!

    private val telemetryListener: (VestTelemetryManager.Telemetry) -> Unit = { telemetry ->
        if (isAdded) updateAlerts(telemetry)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentAlertsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        ThemeManager.applyGlassCards(binding.cardAlerts)
        ThemeManager.applyTextMuted(binding.txtNoAlerts)
        ThemeManager.applyHeading(binding.txtAlertsHeading)

        binding.recyclerAlerts.layoutManager = LinearLayoutManager(requireContext())
        binding.recyclerAlerts.adapter = AlertAdapter(emptyList())
    }

    /**
     * Driven by VestTelemetryManager (Firebase Realtime Database). Uses the
     * same hazard rules (AlertEvaluator) as the web dashboard's "Active
     * Alerts" card and as the notification trigger in MainActivity, so all
     * three always agree on what counts as an active alert.
     */
    private fun updateAlerts(telemetry: VestTelemetryManager.Telemetry) {
        val alerts = AlertEvaluator.evaluate(telemetry)

        binding.recyclerAlerts.adapter = AlertAdapter(alerts)
        binding.txtAlertCount.text = alerts.size.toString()
        binding.txtNoAlerts.visibility = if (alerts.isEmpty()) View.VISIBLE else View.GONE
        binding.recyclerAlerts.visibility = if (alerts.isEmpty()) View.GONE else View.VISIBLE
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
