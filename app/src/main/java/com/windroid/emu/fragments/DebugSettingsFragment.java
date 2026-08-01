package com.windroid.emu.fragments;

import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_LOG;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_LOG_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_NOSIGILL;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_NOSIGILL_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_NOSIGSEGV;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_NOSIGSEGV_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_SHOWBT;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_SHOWBT_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_SHOWSEGV;
import static com.windroid.emu.activities.GeneralSettingsActivity.BOX64_SHOWSEGV_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.PA_SINK_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.PERF_MODE_ROOT;
import static com.windroid.emu.activities.GeneralSettingsActivity.PERF_MODE_ROOT_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELINUX_PERMISSIVE_ROOT;
import static com.windroid.emu.activities.GeneralSettingsActivity.SELINUX_PERMISSIVE_ROOT_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.SPINNER;
import static com.windroid.emu.activities.GeneralSettingsActivity.SWITCH;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_HIDE;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_HIDE_DEFAULT_VALUE;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_LOG_LEVEL;
import static com.windroid.emu.activities.GeneralSettingsActivity.WINE_LOG_LEVEL_DEFAULT_VALUE;
import static com.windroid.emu.activities.MainActivity.CPU_COUNTER;
import static com.windroid.emu.activities.MainActivity.CPU_COUNTER_DEFAULT_VALUE;
import static com.windroid.emu.activities.MainActivity.ENABLE_DEBUG_INFO;
import static com.windroid.emu.activities.MainActivity.ENABLE_DEBUG_INFO_DEFAULT_VALUE;
import static com.windroid.emu.activities.MainActivity.RAM_COUNTER;
import static com.windroid.emu.activities.MainActivity.RAM_COUNTER_DEFAULT_VALUE;
import static com.windroid.emu.activities.MainActivity.deviceArch;

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
import java.util.stream.IntStream;

public class DebugSettingsFragment extends Fragment {
    private final ArrayList<SettingsListSpinner> settingsList = new ArrayList<>();
    private RecyclerView recyclerView;

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

        addToAdapter(R.string.wine_log_level_title, R.string.wine_log_level_desc, new String[] { "disabled", "default" },
                SPINNER, WINE_LOG_LEVEL_DEFAULT_VALUE, WINE_LOG_LEVEL
        );
        addToAdapter(R.string.wine_hide_title, R.string.wine_hide_desc, null,
                SWITCH, WINE_HIDE_DEFAULT_VALUE, WINE_HIDE
        );

        if (!deviceArch.equals("x86_64")) {
            addToAdapter(R.string.box64_log, R.string.box64_log_desc, new String[] { "0", "1" },
                    SPINNER, BOX64_LOG_DEFAULT_VALUE, BOX64_LOG
            );
            addToAdapter(R.string.box64_show_segv, R.string.box64_show_segv_desc, null,
                    SWITCH, BOX64_SHOWSEGV_DEFAULT_VALUE, BOX64_SHOWSEGV
            );
            addToAdapter(R.string.box64_no_sigsegv, R.string.box64_no_sigsegv_desc, null,
                    SWITCH, BOX64_NOSIGSEGV_DEFAULT_VALUE, BOX64_NOSIGSEGV
            );
            addToAdapter(R.string.box64_no_sigill, R.string.box64_no_sigill_desc, null,
                    SWITCH, BOX64_NOSIGILL_DEFAULT_VALUE, BOX64_NOSIGILL
            );
            addToAdapter(R.string.box64_show_bt, R.string.box64_show_bt_desc, null,
                    SWITCH, BOX64_SHOWBT_DEFAULT_VALUE, BOX64_SHOWBT
            );
        }

        addToAdapter(R.string.enable_ram_counter, R.string.enable_ram_counter_desc, null,
                SWITCH, RAM_COUNTER_DEFAULT_VALUE, RAM_COUNTER
        );
        addToAdapter(R.string.enable_cpu_counter, R.string.enable_cpu_counter_desc, null,
                SWITCH, CPU_COUNTER_DEFAULT_VALUE, CPU_COUNTER
        );
        addToAdapter(R.string.enable_debug_info, R.string.enable_debug_info_desc, null,
                SWITCH, ENABLE_DEBUG_INFO_DEFAULT_VALUE, ENABLE_DEBUG_INFO
        );
        addToAdapter(R.string.perf_mode_root_title, R.string.perf_mode_root_desc, null,
                SWITCH, PERF_MODE_ROOT_DEFAULT_VALUE, PERF_MODE_ROOT
        );
        addToAdapter(R.string.selinux_permissive_root_title, R.string.selinux_permissive_root_desc, null,
                SWITCH, SELINUX_PERMISSIVE_ROOT_DEFAULT_VALUE, SELINUX_PERMISSIVE_ROOT
        );
    }

    private void addToAdapter(int titleId, int descriptionId, String[] values, int type, boolean defaultValue, String keyId) {
        addToAdapter(titleId, descriptionId, values, type, String.valueOf(defaultValue), keyId);
    }

    private void addToAdapter(int titleId, int descriptionId, String[] values, int type, int defaultValue, String keyId) {
        addToAdapter(titleId, descriptionId, values, type, String.valueOf(defaultValue), keyId);
    }

    private void addToAdapter(int titleId, int descriptionId, String[] values, int type, String defaultValue, String keyId) {
        settingsList.add(
                new SettingsListSpinner(titleId, descriptionId, values, null, type, defaultValue, keyId)
        );
    }

    public static String[] availableCPUs = IntStream.range(0, Runtime.getRuntime().availableProcessors()).mapToObj(Integer::toString).toArray(String[]::new);
}