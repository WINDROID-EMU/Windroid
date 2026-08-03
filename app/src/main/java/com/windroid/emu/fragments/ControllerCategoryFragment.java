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

public class ControllerCategoryFragment extends Fragment {
    private final ArrayList<SettingsList> settingsList = new ArrayList<>();
    private RecyclerView recyclerView;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_controller_category, container, false);
        recyclerView = rootView.findViewById(R.id.recyclerViewControllerCategory);

        setAdapter();

        return rootView;
    }

    private void setAdapter() {
        recyclerView.setAdapter(new AdapterSettings(settingsList, requireContext()));

        settingsList.clear();

        addToAdapter(R.string.controller_mapper_title, R.string.controller_mapper_desc, R.drawable.ic_joystick);
        addToAdapter(R.string.virtual_controller_mapper_title, R.string.controller_virtual_mapper_desc,
                R.drawable.ic_joystick);
        addToAdapter(R.string.controller_view_title, R.string.controller_view_desc, R.drawable.ic_joystick);
    }

    private void addToAdapter(int titleId, int descriptionId, int iconId) {
        settingsList.add(
                new SettingsList(getString(titleId), getString(descriptionId), iconId));
    }
}
