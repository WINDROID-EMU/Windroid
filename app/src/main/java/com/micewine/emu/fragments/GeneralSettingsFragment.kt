package com.micewine.emu.fragments

import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.RecyclerView
import com.micewine.emu.R
import com.micewine.emu.adapters.AdapterSettings
import com.micewine.emu.adapters.AdapterSettings.SettingsList

class GeneralSettingsFragment : Fragment() {
    private val settingsList: MutableList<SettingsList> = ArrayList()
    private var recyclerView: RecyclerView? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View = inflater.inflate(R.layout.fragment_general_settings, container, false).apply {
        recyclerView = findViewById(R.id.recyclerViewGeneralSettings)
        setAdapter()
    }

    private fun setAdapter() {
        // Limpa a lista antes de adicionar os itens
        settingsList.clear()

        // Verifica se o primeiro ABI suportado não é "x86_64"
        if (Build.SUPPORTED_ABIS.firstOrNull() != "x86_64") {
            addToAdapter(R.string.box64_settings_title, R.string.box64_settings_description, R.drawable.ic_box64)
        }

        addToAdapter(R.string.wine_settings_title, R.string.wine_settings_description, R.drawable.ic_wine)
        addToAdapter(R.string.display_settings_title, R.string.display_settings_description, R.drawable.ic_display)
        addToAdapter(R.string.driver_settings_title, R.string.driver_settings_description, R.drawable.ic_gpu)

        // Define o adapter após popular a lista
        recyclerView?.adapter = AdapterSettings(settingsList, requireContext())
    }

    private fun addToAdapter(titleId: Int, descriptionId: Int, icon: Int) {
        settingsList.add(SettingsList(titleId, descriptionId, icon))
    }
}