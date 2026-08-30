package com.smartvest.command.ui

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.firebase.auth.FirebaseAuth
import com.smartvest.command.databinding.FragmentSettingsBinding
import com.smartvest.command.util.PrefsManager
import com.smartvest.command.util.ThemeManager

class SettingsFragment : Fragment() {

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSettingsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        ThemeManager.applyGlassCards(
            binding.cardTheme, binding.cardAudio, binding.cardNotification, binding.btnLogout
        )
        ThemeManager.applyTextMain(
            binding.lblThemeTitle, binding.lblAudioTitle, binding.lblNotificationTitle
        )
        ThemeManager.applyTextMuted(
            binding.lblThemeDesc, binding.lblAudioDesc, binding.lblNotificationDesc
        )

        binding.switchTheme.isChecked = PrefsManager.isDarkTheme
        binding.switchAudio.isChecked = PrefsManager.isAudioOn
        binding.switchNotification.isChecked = PrefsManager.isNotificationOn

        binding.switchTheme.setOnCheckedChangeListener { _, isChecked ->
            PrefsManager.isDarkTheme = isChecked
            // Recreate so the whole activity (bars, cards, gradients) re-applies the new theme
            activity?.recreate()
        }

        binding.switchAudio.setOnCheckedChangeListener { _, isChecked ->
            PrefsManager.isAudioOn = isChecked
        }

        binding.switchNotification.setOnCheckedChangeListener { _, isChecked ->
            PrefsManager.isNotificationOn = isChecked
        }

        binding.btnLogout.setOnClickListener {
            FirebaseAuth.getInstance().signOut()
            PrefsManager.logout()
            val intent = Intent(requireContext(), LoginActivity::class.java)
            intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            startActivity(intent)
            activity?.finish()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
