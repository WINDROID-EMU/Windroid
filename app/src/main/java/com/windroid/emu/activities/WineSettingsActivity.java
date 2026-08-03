package com.windroid.emu.activities;

import static com.windroid.emu.fragments.CreatePresetFragment.WINE_PREFIX_PRESET;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ImageButton;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;
import com.windroid.emu.R;
import com.windroid.emu.adapters.AdapterTabPagerWineSettings;
import com.windroid.emu.databinding.ActivityWineSettingsBinding;
import com.windroid.emu.fragments.CreatePresetFragment;

/**
 * Tela unificada "Configurações do Wine": junta em um só lugar o Gerenciador de
 * Prefixos Wine, as Configurações do Wine e o Winetricks, cada um em sua própria aba.
 */
public class WineSettingsActivity extends AppCompatActivity {
    private FloatingActionButton addWinePrefixFAB;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ActivityWineSettingsBinding binding = ActivityWineSettingsBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        ImageButton backButton = findViewById(R.id.backButton);
        backButton.setOnClickListener((v) -> onKeyDown(KeyEvent.KEYCODE_BACK, null));

        Toolbar wineSettingsToolbar = findViewById(R.id.wineSettingsToolbar);
        wineSettingsToolbar.setTitle(R.string.wine_unified_settings_title);

        TabLayout tabLayout = findViewById(R.id.wineSettingsTabLayout);
        ViewPager2 viewPager = findViewById(R.id.wineSettingsViewPager);
        addWinePrefixFAB = findViewById(R.id.addWinePrefixFAB);

        viewPager.setAdapter(new AdapterTabPagerWineSettings(this));

        String[] tabTitles = {
                getString(R.string.wine_tab_prefixes_title),
                getString(R.string.wine_tab_settings_title),
                getString(R.string.winetricks_title)
        };

        new TabLayoutMediator(tabLayout, viewPager,
                (tab, position) -> tab.setText(tabTitles[position])).attach();

        addWinePrefixFAB.setOnClickListener((v) ->
                new CreatePresetFragment(WINE_PREFIX_PRESET).show(getSupportFragmentManager(), ""));

        viewPager.registerOnPageChangeCallback(new ViewPager2.OnPageChangeCallback() {
            @Override
            public void onPageSelected(int position) {
                addWinePrefixFAB.setVisibility(position == 0 ? View.VISIBLE : View.GONE);
            }
        });
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            finish();
        }

        return true;
    }
}
