package com.windroid.emu.fragments;

import static com.windroid.emu.activities.GeneralSettingsActivity.COLOR_PROFILE;
import static com.windroid.emu.activities.GeneralSettingsActivity.COLOR_PROFILE_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.FPS_LIMIT;
import static com.windroid.emu.activities.GeneralSettingsActivity.SEEKBAR;
import static com.windroid.emu.activities.GeneralSettingsActivity.SPINNER;
import static com.windroid.emu.activities.GeneralSettingsActivity.SUPER_RESOLUTION;
import static com.windroid.emu.activities.GeneralSettingsActivity.SUPER_RESOLUTION_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SWITCH;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FSR_MODE;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FSR_MODE_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FSR_SHARPNESS;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FSR_SHARPNESS_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_WINE_FRAMESKIP;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FRAMESKIP_0;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FRAMESKIP_1;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FRAMESKIP_2;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_FRAMESKIP_3;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.windroid.emu.R;
import com.windroid.emu.adapters.AdapterSettingsPreferences;
import com.windroid.emu.adapters.AdapterSettingsPreferences.SettingsListSpinner;

import java.util.ArrayList;

public class GraphicEngineSettingsFragment extends Fragment {
    private RecyclerView recyclerView;
    private final ArrayList<SettingsListSpinner> settingsList = new ArrayList<>();

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_settings_model, container, false);

        recyclerView = rootView.findViewById(R.id.recyclerViewSettingsModel);

        GridLayoutManager layoutManager = (GridLayoutManager) recyclerView.getLayoutManager();
        if (layoutManager != null) {
            layoutManager.setSpanCount(1);
        }

        setAdapter();

        return rootView;
    }

    private void setAdapter() {
        recyclerView.setAdapter(new AdapterSettingsPreferences(settingsList, requireActivity()));

        settingsList.clear();

        addToAdapter(R.string.fps_limit_title, R.string.null_desc, null, new int[]{0, 120}, SEEKBAR, "0", FPS_LIMIT);
        addToAdapter(R.string.super_resolution_title, R.string.null_desc, null, null, SWITCH, String.valueOf(SUPER_RESOLUTION_DEFAULT_VALUE), SUPER_RESOLUTION);
        addToAdapter(R.string.wine_fsr_mode_title, R.string.null_desc, new String[]{"Ultra Quality", "Quality", "Balanced", "Performance"}, null, SPINNER, WINE_FSR_MODE_DEFAULT_VALUE, WINE_FSR_MODE);
        addToAdapter(R.string.wine_fsr_sharpness_title, R.string.null_desc, null, new int[]{0, 5}, SEEKBAR, String.valueOf(WINE_FSR_SHARPNESS_DEFAULT_VALUE), WINE_FSR_SHARPNESS);
        addToAdapter(R.string.wine_frameskip_title, R.string.wine_frameskip_desc, new String[]{WINE_FRAMESKIP_0, WINE_FRAMESKIP_1, WINE_FRAMESKIP_2, WINE_FRAMESKIP_3}, null, SPINNER, WINE_FRAMESKIP_0, SELECTED_WINE_FRAMESKIP);
        addToAdapter(R.string.color_profile_title, R.string.null_desc, new String[]{"Neutral", "Vivid", "Warm", "Cool"}, null, SPINNER, COLOR_PROFILE_DEFAULT_VALUE, COLOR_PROFILE);
    }

    private void addToAdapter(int titleId, int descriptionId, String[] spinnerOptions, int[] seekBarValues, int type, String defaultValue, String keyId) {
        settingsList.add(
                new SettingsListSpinner(titleId, descriptionId, spinnerOptions, seekBarValues, type, defaultValue, keyId)
        );
    }
}
