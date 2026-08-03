package com.windroid.emu.activities;

import android.os.Bundle;
import android.view.KeyEvent;
import android.widget.ImageButton;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

import com.windroid.emu.R;
import com.windroid.emu.databinding.ActivityControllerCategoryBinding;
import com.windroid.emu.fragments.ControllerCategoryFragment;

public class ControllerCategoryActivity extends AppCompatActivity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ActivityControllerCategoryBinding binding = ActivityControllerCategoryBinding.inflate(getLayoutInflater());

        setContentView(binding.getRoot());

        Toolbar controllerCategoryToolbar = findViewById(R.id.controllerCategoryTitle);
        controllerCategoryToolbar.setTitle(getString(R.string.controller_settings_title));

        ImageButton backButton = findViewById(R.id.backButton);

        backButton.setOnClickListener((v) -> onKeyDown(KeyEvent.KEYCODE_BACK, null));

        fragmentLoader(new ControllerCategoryFragment());
    }

    private void fragmentLoader(Fragment fragment) {
        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();

        transaction.setCustomAnimations(
                R.anim.slide_in,
                R.anim.fade_out,
                R.anim.fade_in,
                R.anim.slide_out
        );

        transaction.replace(R.id.controller_category_content, fragment);
        transaction.commit();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            finish();
        }

        return true;
    }
}
