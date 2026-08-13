package com.windroid.emu.fragments;

import static com.windroid.emu.activities.GeneralSettingsActivity.CHECKBOX;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_AFME;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_AFME_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_DRI3;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_DRI3_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_DXVK_HUD_PRESET;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_DXVK_HUD_PRESET_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_GL_PROFILE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_GL_PROFILE_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_MESA_VK_WSI_PRESENT_MODE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_MESA_VK_WSI_PRESENT_MODE_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_SCALING_FILTER;
import static com.windroid.emu.activities.GeneralSettingsActivity.SCALING_FILTER_LINEAR;
import static com.windroid.emu.activities.GeneralSettingsActivity.SCALING_FILTER_FSR;
import static com.windroid.emu.activities.GeneralSettingsActivity.SCALING_FILTER_CAS;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_FRAME_GENERATION;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAME_GENERATION_OFF;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAME_GENERATION_SMOOTHING;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_FRAMESKIP;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAMESKIP_0;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAMESKIP_1;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAMESKIP_2;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAMESKIP_3;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAMESKIP_4;
import static com.windroid.emu.activities.GeneralSettingsActivity.FRAMESKIP_5;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_TU_DEBUG_PRESET;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_TU_DEBUG_PRESET_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_TU_TEXTURE_LOD_BIAS;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_TU_TEXTURE_LOD_BIAS_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_TU_FORCE_MIP_LEVEL;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_TU_FORCE_MIP_LEVEL_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_FORCE_SHADING_RATE;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_FORCE_SHADING_RATE_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_FORCE_SYSMEM;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_FORCE_SYSMEM_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_DISABLE_SHADOWS;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_DISABLE_SHADOWS_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_DISABLE_HEAVY_EFFECTS;
import static com.windroid.emu.activities.GeneralSettingsActivity.ENABLE_TU_DISABLE_HEAVY_EFFECTS_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_VRAM_LIMIT;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELECTED_VRAM_LIMIT_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SPINNER;
import static com.windroid.emu.activities.GeneralSettingsActivity.SWITCH;

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

import java.util.ArrayList;

public class DriversSettingsFragment extends Fragment {
        private final ArrayList<AdapterSettingsPreferences.SettingsListSpinner> settingsList = new ArrayList<>();
        private RecyclerView recyclerView;

        @Nullable
        @Override
        public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                        @Nullable Bundle savedInstanceState) {
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

                addToAdapter(R.string.enable_dri3, R.string.null_desc, null, SWITCH,
                                String.valueOf(ENABLE_DRI3_DEFAULT_VALUE),
                                ENABLE_DRI3);
                addToAdapter(R.string.select_dxvk_hud_preset_title, R.string.null_desc,
                                new String[] { "fps", "gpuload", "devinfo", "version", "api", "memory", "cs",
                                                "compiler", "allocations",
                                                "pipelines", "frametimes", "descriptors", "drawcalls", "submissions" },
                                CHECKBOX, SELECTED_DXVK_HUD_PRESET_DEFAULT_VALUE, SELECTED_DXVK_HUD_PRESET);
                addToAdapter(R.string.mesa_vk_wsi_present_mode_title, R.string.null_desc,
                                new String[] { "fifo", "relaxed", "mailbox", "immediate" }, SPINNER,
                                SELECTED_MESA_VK_WSI_PRESENT_MODE_DEFAULT_VALUE, SELECTED_MESA_VK_WSI_PRESENT_MODE);
                addToAdapter(
                                R.string.tu_debug_title, R.string.null_desc,
                                new String[] { "noconform", "flushall", "syncdraw",
                                                "sysmem", "gmem", "nolrz", "noubwc", "nomultipos", "forcebin" },
                                CHECKBOX, SELECTED_TU_DEBUG_PRESET_DEFAULT_VALUE, SELECTED_TU_DEBUG_PRESET);
                addToAdapter(
                                R.string.tu_texture_lod_bias_title, R.string.tu_texture_lod_bias_desc,
                                new String[] { "Off", "0.5", "1.0", "1.5", "2.0", "2.5", "3.0" },
                                SPINNER, SELECTED_TU_TEXTURE_LOD_BIAS_DEFAULT_VALUE, SELECTED_TU_TEXTURE_LOD_BIAS);
                addToAdapter(
                                R.string.tu_force_mip_level_title, R.string.tu_force_mip_level_desc,
                                new String[] { "Off", "50% (1)", "25% (2)", "12.5% (3)" },
                                SPINNER, SELECTED_TU_FORCE_MIP_LEVEL_DEFAULT_VALUE, SELECTED_TU_FORCE_MIP_LEVEL);
                addToAdapter(R.string.tu_force_shading_rate_title, R.string.tu_force_shading_rate_desc, null, SWITCH,
                                String.valueOf(ENABLE_TU_FORCE_SHADING_RATE_DEFAULT_VALUE),
                                ENABLE_TU_FORCE_SHADING_RATE);
                addToAdapter(R.string.tu_force_sysmem_title, R.string.tu_force_sysmem_desc, null, SWITCH,
                                String.valueOf(ENABLE_TU_FORCE_SYSMEM_DEFAULT_VALUE),
                                ENABLE_TU_FORCE_SYSMEM);
                addToAdapter(R.string.tu_disable_shadows_title, R.string.tu_disable_shadows_desc, null, SWITCH,
                                String.valueOf(ENABLE_TU_DISABLE_SHADOWS_DEFAULT_VALUE),
                                ENABLE_TU_DISABLE_SHADOWS);
                addToAdapter(R.string.tu_disable_heavy_effects_title, R.string.tu_disable_heavy_effects_desc, null, SWITCH,
                                String.valueOf(ENABLE_TU_DISABLE_HEAVY_EFFECTS_DEFAULT_VALUE),
                                ENABLE_TU_DISABLE_HEAVY_EFFECTS);
                addToAdapter(R.string.select_gl_profile_title, R.string.null_desc,
                                new String[] {
                                                "GL 2.1", "GL 3.0",
                                                "GL 3.1", "GL 3.2",
                                                "GL 3.3", "GL 4.0",
                                                "GL 4.1", "GL 4.2",
                                                "GL 4.3", "GL 4.4",
                                                "GL 4.5", "GL 4.6"
                                },
                                SPINNER, SELECTED_GL_PROFILE_DEFAULT_VALUE, SELECTED_GL_PROFILE);
                addToAdapter(R.string.vram_limit_title, R.string.vram_limit_desc,
                                new String[] { "Auto", "128 MB", "256 MB", "512 MB", "1024 MB", "2048 MB", "4096 MB" },
                                SPINNER, SELECTED_VRAM_LIMIT_DEFAULT_VALUE, SELECTED_VRAM_LIMIT);
                addToAdapter(R.string.enable_afme_title, R.string.enable_afme_description, null, SWITCH,
                                String.valueOf(ENABLE_AFME_DEFAULT_VALUE),
                                ENABLE_AFME);
                addToAdapter(R.string.scaling_filter_title, R.string.scaling_filter_desc,
                                new String[] { SCALING_FILTER_LINEAR, SCALING_FILTER_FSR, SCALING_FILTER_CAS },
                                SPINNER, SCALING_FILTER_LINEAR, SELECTED_SCALING_FILTER);
                addToAdapter(R.string.frame_generation_title, R.string.frame_generation_desc,
                                new String[] { FRAME_GENERATION_OFF, FRAME_GENERATION_SMOOTHING },
                                SPINNER, FRAME_GENERATION_OFF, SELECTED_FRAME_GENERATION);

                addToAdapter(R.string.frameskip_title, R.string.frameskip_desc,
                                new String[] { FRAMESKIP_0, FRAMESKIP_1, FRAMESKIP_2, FRAMESKIP_3, FRAMESKIP_4, FRAMESKIP_5 },
                                SPINNER, FRAMESKIP_0, SELECTED_FRAMESKIP);
        }

        private void addToAdapter(int titleId, int descriptionId, String[] values, int type, String defaultValue,
                        String keyId) {
                settingsList.add(
                                new AdapterSettingsPreferences.SettingsListSpinner(titleId, descriptionId, values, null,
                                                type,
                                                defaultValue, keyId));
        }
}