package com.windroid.emu.fragments;

import static com.windroid.emu.activities.MainActivity.setSharedVars;
import static com.windroid.emu.activities.MainActivity.usrDir;
import static com.windroid.emu.activities.RatManagerActivity.generateICDFile;
import static com.windroid.emu.core.EnvVars.getEnv;
import static com.windroid.emu.core.RatPackageManager.getPackageById;
import static com.windroid.emu.core.RatPackageManager.listRatPackages;
import static com.windroid.emu.core.RatPackageManager.listRatPackagesId;
import static com.windroid.emu.core.ShellLoader.runCommand;
import static com.windroid.emu.core.ShellLoader.runCommandWithOutput;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.windroid.emu.R;
import com.windroid.emu.core.RatPackageManager.RatPackage;

import java.io.File;

import java.util.List;
import java.util.stream.Collectors;

public class DriverInfoFragment extends Fragment {
    private Thread vulkanInfoThread = null;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_driver_info, container, false);

        Spinner driverSpinner = rootView.findViewById(R.id.driverSpinner);
        TextView driverInfoText = rootView.findViewById(R.id.logsTextView);
        ScrollView scrollView = rootView.findViewById(R.id.scrollView);

        List<String> vulkanDriversId = listRatPackagesId("VulkanDriver", "AdrenoToolsDriver");
        List<RatPackage> vulkanDrivers = listRatPackages("VulkanDriver", "AdrenoToolsDriver");
        List<String> vulkanDriversStr = vulkanDrivers.stream().map(p -> p.getName() + " " + p.getVersion())
                .collect(Collectors.toList());

        driverSpinner.setAdapter(
                new ArrayAdapter<>(requireContext(), android.R.layout.simple_spinner_dropdown_item, vulkanDriversStr));
        driverSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                String driverId = vulkanDriversId.get(driverSpinner.getSelectedItemPosition());

                String driverPath;
                String adrenoToolsDriverPath = null;

                if (driverId.contains("AdrenoToolsDriver")) {
                    try {
                        RatPackage adrenoToolsWrapper = listRatPackages("AdrenoTools").get(0);
                        RatPackage ratPackage = getPackageById(driverId);
                        driverPath = (adrenoToolsWrapper != null ? adrenoToolsWrapper.getDriverLib() : null);
                        adrenoToolsDriverPath = (ratPackage != null ? ratPackage.getDriverLib() : null);
                    } catch (IndexOutOfBoundsException e) {
                        Toast.makeText(requireContext(), "AdrenoTools Provider Not Found", Toast.LENGTH_SHORT).show();
                        return;
                    }
                } else {
                    RatPackage ratPackage = getPackageById(driverId);
                    driverPath = (ratPackage != null ? ratPackage.getDriverLib() : null);
                }

                setSharedVars(requireActivity(), adrenoToolsDriverPath);

                // Validate driver library path before generating ICD file
                if (driverPath == null || driverPath.isEmpty()) {
                    Log.e("DriverInfoFragment", "Driver library path is null or empty for driver: " + driverId);
                    Toast.makeText(requireContext(), "Error: Driver library path not found", Toast.LENGTH_LONG).show();
                    return;
                }

                File driverFile = new File(driverPath);
                if (!driverFile.exists()) {
                    Log.e("DriverInfoFragment", "Driver library file does not exist: " + driverPath);
                    Toast.makeText(requireContext(), "Error: Driver file not found at " + driverPath, Toast.LENGTH_LONG).show();
                    return;
                }

                generateICDFile(driverPath);

                vulkanInfoThread = new Thread(() -> {
                    try {
                        String vulkanInfoBin = usrDir + "/bin/vulkaninfo";
                        runCommand("chmod +x " + vulkanInfoBin, false);
                        
                        // Add timeout to prevent hanging on driver crashes
                        String driverInfo = runCommandWithOutput("timeout 10 " + getEnv() + vulkanInfoBin, true);

                        if (isAdded() && !isDetached()) {
                            driverInfoText.post(() -> {
                                if (isAdded() && !isDetached()) {
                                    ViewPropertyAnimator animator = driverInfoText.animate();
                                    animator.alpha(0F);
                                    animator.setDuration(100L);
                                    animator.withEndAction(() -> {
                                        driverInfoText.setText(driverInfo);
                                        animator.alpha(1F);
                                        animator.setDuration(100L);
                                        animator.start();
                                    });
                                    animator.start();

                                    scrollView.scrollTo(0, 0);
                                }
                            });
                        }
                    } catch (Exception e) {
                        Log.e("DriverInfoFragment", "Error running vulkaninfo", e);
                        if (isAdded() && !isDetached()) {
                            driverInfoText.post(() -> {
                                if (isAdded() && !isDetached()) {
                                    String errorMsg = "Error: Failed to get Vulkan driver info.\n\n" +
                                            "The driver may have crashed or is incompatible.\n" +
                                            "This is common with AdrenoTools drivers on some devices.\n\n" +
                                            "Try:\n" +
                                            "- Using a different Vulkan driver\n" +
                                            "- Disabling AdrenoTools\n" +
                                            "- Checking device compatibility\n\n" +
                                            "Error details: " + e.getMessage();
                                    driverInfoText.setText(errorMsg);
                                }
                            });
                        }
                    }
                });
                vulkanInfoThread.start();
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });

        return rootView;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (vulkanInfoThread != null && vulkanInfoThread.isAlive()) {
            vulkanInfoThread.interrupt();
        }
    }
}