package com.windroid.emu.fragments;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.RecyclerView;
import com.windroid.emu.R;
import com.windroid.emu.adapters.AdapterSettings;
import com.windroid.emu.adapters.AdapterSettings.SettingsList;

import java.util.ArrayList;

public class GeneralSettingsFragment extends Fragment {
    private final ArrayList<SettingsList> settingsList = new ArrayList<>();
    private RecyclerView recyclerView;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_general_settings, container, false);
        recyclerView = rootView.findViewById(R.id.recyclerViewGeneralSettings);

        setAdapter();

        return rootView;
    }

    private void setAdapter() {
        recyclerView.setAdapter(new AdapterSettings(settingsList, requireContext()));

        settingsList.clear();

        addToAdapter(R.string.debug_settings_title, R.string.debug_settings_desc, R.drawable.ic_settings_outline);
        addToAdapter(R.string.sound_settings_title, R.string.sound_settings_desc, R.drawable.ic_sound);
        addToAdapter(R.string.driver_settings_title, R.string.driver_settings_desc, R.drawable.ic_gpu);
        addToAdapter(R.string.driver_info_title, R.string.driver_info_desc, R.drawable.ic_gpu);
        addToAdapter(R.string.env_settings_title, R.string.env_settings_desc, R.drawable.ic_globe);
        addToAdapter(R.string.wine_settings_title, R.string.wine_settings_desc, R.drawable.ic_wine);
        addToAdapter(R.string.winetricks_title, R.string.winetricks_desc, R.drawable.ic_wine);
        addToAdapter(R.string.graphic_engine_settings_title, R.string.graphic_engine_settings_desc, R.drawable.ic_settings_outline);
        addToAdapter(R.string.scan_games_title, R.string.scan_games_desc, R.drawable.ic_folder);
    }

    private void addToAdapter(int titleId, int descriptionId, int iconId) {
        settingsList.add(
                new SettingsList(getString(titleId), getString(descriptionId), iconId));
    }
}
