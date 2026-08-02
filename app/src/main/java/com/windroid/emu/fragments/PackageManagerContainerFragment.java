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

public class PackageManagerContainerFragment extends Fragment {
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

        addToAdapter(R.string.rat_manager_title, R.string.rat_manager_desc, R.drawable.ic_rat_package_grayscale);
        addToAdapter(R.string.rat_downloader_title, R.string.rat_downloader_desc, R.drawable.ic_download);
    }

    private void addToAdapter(int titleId, int descriptionId, int iconId) {
        settingsList.add(
                new SettingsList(getString(titleId), getString(descriptionId), iconId));
    }
}
