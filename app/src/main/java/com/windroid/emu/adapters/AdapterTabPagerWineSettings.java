package com.windroid.emu.adapters;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import com.windroid.emu.fragments.WinePrefixManagerFragment;
import com.windroid.emu.fragments.WineSettingsFragment;
import com.windroid.emu.fragments.WinetricksFragment;

/**
 * Junta o Gerenciador de Prefixos Wine, as Configurações do Wine e o Winetricks
 * em uma única tela, cada um como uma aba do ViewPager2.
 */
public class AdapterTabPagerWineSettings extends FragmentStateAdapter {
    public AdapterTabPagerWineSettings(@NonNull FragmentActivity fragmentActivity) {
        super(fragmentActivity);
    }

    @NonNull
    @Override
    public Fragment createFragment(int position) {
        return switch (position) {
            case 0 -> new WinePrefixManagerFragment();
            case 1 -> new WineSettingsFragment();
            case 2 -> new WinetricksFragment();
            default -> throw new IllegalArgumentException("Invalid Fragment for Position " + position);
        };
    }

    @Override
    public int getItemCount() {
        return 3;
    }
}
