package com.windroid.emu.fragments;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.windroid.emu.R;
import com.windroid.emu.adapters.AdapterWinetricks;
import com.windroid.emu.core.ShellLoader;
import com.windroid.emu.core.WinetricksCache;
import com.windroid.emu.core.WinetricksItem;
import com.windroid.emu.core.WinetricksWrapper;

import static com.windroid.emu.activities.MainActivity.winePrefixesDir;
import static com.windroid.emu.activities.MainActivity.winePrefix;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class WinetricksFragment extends Fragment implements ShellLoader.LogCallback {

    private EditText winetricksSearch;
    private AdapterWinetricks adapter;
    private Button btnRunWinetricks;
    private TextView winetricksStatusText;
    private ProgressBar winetricksProgressBar;
    private final List<WinetricksItem> packageList = new ArrayList<>();
    private final Pattern progressPattern = Pattern.compile("(\\d+)%");
    private boolean isFetchingList = false;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_winetricks, container, false);

        winetricksSearch = rootView.findViewById(R.id.winetricksSearch);
        RecyclerView winetricksRecyclerView = rootView.findViewById(R.id.winetricksRecyclerView);
        btnRunWinetricks = rootView.findViewById(R.id.btnRunWinetricks);
        Button btnOpenLogViewer = rootView.findViewById(R.id.btnOpenLogViewer);
        Button btnRefreshList = rootView.findViewById(R.id.btnRefreshList);
        winetricksStatusText = rootView.findViewById(R.id.winetricksStatusText);
        winetricksProgressBar = rootView.findViewById(R.id.winetricksProgressBar);

        winetricksRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        adapter = new AdapterWinetricks(packageList);
        winetricksRecyclerView.setAdapter(adapter);

        winetricksSearch.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                adapter.filter(s.toString());
            }
            @Override
            public void afterTextChanged(Editable s) {}
        });

        btnRunWinetricks.setOnClickListener(v -> {
            List<WinetricksItem> selected = adapter.getSelectedItems();
            if (selected.isEmpty()) {
                Toast.makeText(getContext(), "Please select at least one item", Toast.LENGTH_SHORT).show();
                return;
            }

            StringBuilder args = new StringBuilder();
            for (WinetricksItem item : selected) {
                args.append(item.getName()).append(" ");
            }

            runWinetricks(args.toString().trim());
        });

        btnOpenLogViewer.setOnClickListener(v -> requireActivity().getSupportFragmentManager().beginTransaction()
                .replace(R.id.settings_content, new LogViewerFragment())
                .addToBackStack(null)
                .commit());

        btnRefreshList.setOnClickListener(v -> {
            WinetricksCache.clearCache(requireContext());
            packageList.clear();
            adapter.updateList();
            fetchWinetricksList();
        });

        fetchWinetricksList();

        return rootView;
    }

    @SuppressLint("SetTextI18n")
    private void fetchWinetricksList() {
        if (isFetchingList) return;
        isFetchingList = true;

        // Tenta carregar do cache primeiro
        if (WinetricksCache.hasValidCache(requireContext())) {
            List<WinetricksItem> cachedItems = WinetricksCache.loadFromCache(requireContext());
            if (!cachedItems.isEmpty()) {
                packageList.clear();
                packageList.addAll(cachedItems);
                adapter.updateList();
                isFetchingList = false;
                // Verifica instalados em background
                new Thread(() -> {
                    checkInstalledPackages();
                    new Handler(Looper.getMainLooper()).post(() -> adapter.updateList());
                }).start();
                return;
            }
        }

        setRunningState(true);
        winetricksStatusText.setText("Fetching package list...");
        winetricksProgressBar.setIndeterminate(true);

        new Thread(() -> {
            // We use a specific log collector for the list
            final StringBuilder listOutput = new StringBuilder();
            ShellLoader.LogCallback listCollector = listOutput::append;
            
            ShellLoader.connectOutput(listCollector);
            WinetricksWrapper.winetricks("list-all");
            ShellLoader.cleanup();

            parseListOutput(listOutput.toString());

            // Salva no cache
            if (!packageList.isEmpty()) {
                WinetricksCache.saveToCache(requireContext(), packageList);
            }

            new Handler(Looper.getMainLooper()).post(() -> {
                setRunningState(false);
                adapter.updateList();
                isFetchingList = false;
                if (packageList.isEmpty()) {
                    Toast.makeText(getContext(), "Failed to load Winetricks list", Toast.LENGTH_LONG).show();
                } else {
                    // Verifica instalados em background após carregar
                    new Thread(() -> {
                        checkInstalledPackages();
                        new Handler(Looper.getMainLooper()).post(() -> adapter.updateList());
                    }).start();
                }
            });
        }).start();
    }

    private void parseListOutput(String output) {
        packageList.clear();
        String[] lines = output.split("\n");
        String currentCategory = "General";
        Context context = requireContext();

        for (String line : lines) {
            line = line.trim();
            if (line.isEmpty()) continue;

            if (!line.contains(" ")) {
                currentCategory = line;
                continue;
            }

            int firstSpace = line.indexOf(" ");
            String name = line.substring(0, firstSpace).trim();
            String description = line.substring(firstSpace).trim();

            if (name.equals("---------------------------------------------------------")) continue;

            String simpleName = WinetricksItem.getSimpleNameForPackage(name);
            int iconResId = WinetricksItem.getIconForPackage(name, context);
            
            packageList.add(new WinetricksItem(name, description, currentCategory, simpleName, iconResId));
        }
    }

    private void checkInstalledPackages() {
        // Verifica quais pacotes já estão instalados no prefixo atual
        for (WinetricksItem item : packageList) {
            boolean installed = isPackageInstalled(item.getName());
            item.setInstalled(installed);
        }
    }

    private boolean isPackageInstalled(String packageName) {
        // Verifica se o pacote está instalado verificando o winetricks.log
        String prefix = winePrefixesDir + "/" + winePrefix;
        String command = "cat " + prefix + "/winetricks.log 2>/dev/null | grep '^" + packageName + "$'";
        String output = ShellLoader.runCommandWithOutput(command, false);
        return output != null && output.trim().equals(packageName);
    }

    @SuppressLint("SetTextI18n")
    private void runWinetricks(String args) {
        setRunningState(true);
        winetricksStatusText.setText("Executing Winetricks...");
        winetricksProgressBar.setIndeterminate(true);
        ShellLoader.connectOutput(this);

        new Thread(() -> {
            int result = WinetricksWrapper.winetricks(args);

            new Handler(Looper.getMainLooper()).post(() -> {
                setRunningState(false);
                if (result == 0) {
                    Toast.makeText(getContext(), "Winetricks Finished successfully", Toast.LENGTH_LONG).show();
                } else {
                    Toast.makeText(getContext(), "Winetricks Failed (Exit code: " + result + ")", Toast.LENGTH_LONG).show();
                }
            });
        }).start();
    }

    private void setRunningState(boolean running) {
        btnRunWinetricks.setEnabled(!running);
        winetricksProgressBar.setVisibility(running ? View.VISIBLE : View.GONE);
        winetricksStatusText.setVisibility(running ? View.VISIBLE : View.GONE);
        winetricksSearch.setEnabled(!running);
    }

    @SuppressLint("SetTextI18n")
    @Override
    public void appendLogs(String text) {
        if (getActivity() == null) return;

        getActivity().runOnUiThread(() -> {
            if (text.contains("Downloading")) {
                winetricksStatusText.setText("Downloading...");
                winetricksProgressBar.setIndeterminate(true);
            } else if (text.contains("Extracting")) {
                winetricksStatusText.setText("Extracting...");
                winetricksProgressBar.setIndeterminate(true);
            } else if (text.contains("Installing")) {
                winetricksStatusText.setText("Installing...");
                winetricksProgressBar.setIndeterminate(true);
            } else if (text.contains("Setting up")) {
                winetricksStatusText.setText("Configuring...");
                winetricksProgressBar.setIndeterminate(true);
            }

            Matcher matcher = progressPattern.matcher(text);
            if (matcher.find()) {
                try {
                    int progress = Integer.parseInt(Objects.requireNonNull(matcher.group(1)));
                    winetricksProgressBar.setIndeterminate(false);
                    winetricksProgressBar.setProgress(progress);
                    winetricksStatusText.setText("Downloading... " + progress + "%");
                } catch (NumberFormatException ignored) {
                }
            }
        });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        ShellLoader.cleanup();
    }
}
