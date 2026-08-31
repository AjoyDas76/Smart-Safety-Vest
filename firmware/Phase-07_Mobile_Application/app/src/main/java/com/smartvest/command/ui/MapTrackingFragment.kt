package com.smartvest.command.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.google.android.gms.maps.CameraUpdateFactory
import com.google.android.gms.maps.GoogleMap
import com.google.android.gms.maps.OnMapReadyCallback
import com.google.android.gms.maps.SupportMapFragment
import com.google.android.gms.maps.model.BitmapDescriptorFactory
import com.google.android.gms.maps.model.LatLng
import com.google.android.gms.maps.model.MarkerOptions
import com.smartvest.command.databinding.FragmentMapTrackingBinding
import com.smartvest.command.util.VestTelemetryManager

class MapTrackingFragment : Fragment(), OnMapReadyCallback {

    private var _binding: FragmentMapTrackingBinding? = null
    private val binding get() = _binding!!

    private var googleMap: GoogleMap? = null
    private var workerMarker: com.google.android.gms.maps.model.Marker? = null

    // The vest's actual last-known GPS fix (worker1/gps/latitude,longitude in
    // Firebase, read by VestTelemetryManager). Null until the first real fix
    // arrives — no hardcoded/simulated starting point, so the map never shows
    // a location that isn't really the worker's.
    private var currentLatLng: LatLng? = null

    private val telemetryListener: (VestTelemetryManager.Telemetry) -> Unit = { telemetry ->
        if (isAdded) updateFromTelemetry(telemetry)
    }

    private val locationPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) enableMyLocation()
        }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentMapTrackingBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val mapFragment = childFragmentManager.findFragmentById(
            com.smartvest.command.R.id.mapContainer
        ) as? SupportMapFragment
        mapFragment?.getMapAsync(this)

        binding.btnCloseMap.setOnClickListener {
            (activity as? MainActivity)?.closeFullscreenMap()
        }

        if (ContextCompat.checkSelfPermission(
                requireContext(), Manifest.permission.ACCESS_FINE_LOCATION
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            locationPermissionLauncher.launch(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    override fun onMapReady(map: GoogleMap) {
        googleMap = map
        map.uiSettings.isZoomControlsEnabled = true

        // Satellite (imagery + road/place labels) view, as requested.
        map.mapType = GoogleMap.MAP_TYPE_HYBRID

        if (ContextCompat.checkSelfPermission(
                requireContext(), Manifest.permission.ACCESS_FINE_LOCATION
            ) == PackageManager.PERMISSION_GRANTED
        ) {
            enableMyLocation()
        }

        // In case a telemetry update already arrived before the map finished loading.
        currentLatLng?.let { placeOrMoveMarker(it, snapCamera = true) }
    }

    @android.annotation.SuppressLint("MissingPermission")
    private fun enableMyLocation() {
        googleMap?.isMyLocationEnabled = true
    }

    /**
     * Driven by VestTelemetryManager (Firebase Realtime Database:
     * worker1/gps/latitude, worker1/gps/longitude) — the vest's real GPS fix,
     * not a simulated one. The marker only ever moves to a fix that actually
     * came from the vest.
     */
    private fun updateFromTelemetry(telemetry: VestTelemetryManager.Telemetry) {
        val lat = telemetry.latitude
        val lng = telemetry.longitude

        if (lat == null || lng == null) {
            // No GPS fix yet — leave any previously-placed marker where it is
            // (last known location) rather than guessing a new one.
            if (workerMarker == null) binding.txtMapCoords.text = "--"
            return
        }

        val newLatLng = LatLng(lat, lng)
        val isFirstFix = currentLatLng == null
        currentLatLng = newLatLng

        placeOrMoveMarker(newLatLng, snapCamera = isFirstFix)
        updateCoordsLabel(newLatLng)
    }

    private fun placeOrMoveMarker(latLng: LatLng, snapCamera: Boolean) {
        val map = googleMap ?: return

        val marker = workerMarker
        if (marker == null) {
            workerMarker = map.addMarker(
                MarkerOptions()
                    .position(latLng)
                    .title("Worker: Rahim Uddin")
                    .snippet("Vest ID: SV-2026-014")
                    .icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_AZURE))
            )
            map.moveCamera(CameraUpdateFactory.newLatLngZoom(latLng, 16f))
        } else {
            marker.position = latLng
            if (snapCamera) {
                map.moveCamera(CameraUpdateFactory.newLatLngZoom(latLng, 16f))
            } else {
                map.animateCamera(CameraUpdateFactory.newLatLng(latLng))
            }
        }
    }

    private fun updateCoordsLabel(latLng: LatLng) {
        binding.txtMapCoords.text = String.format(
            "%.5f, %.5f", latLng.latitude, latLng.longitude
        )
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
