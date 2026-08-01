package com.windroid.emu.controller;

import android.content.Context;
import android.os.VibrationEffect;
import android.os.Vibrator;

public class VibratorHelper {
    private static final long[] PATTERN = {0, 16};
    private static final int[] AMPLITUDES = {0, 255};
    private static Vibrator vibrator;
    private static boolean vibrationEnabled = true;

    public static void initialize(Context context) {
        if (vibrator == null) {
            vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        }
    }

    public static void setVibrationEnabled(boolean enabled) {
        vibrationEnabled = enabled;
        if (!enabled && vibrator != null) {
            vibrator.cancel();
        }
        // Update native side
        ControllerUtils.setVibrationEnabled(enabled);
    }

    public static boolean isVibrationEnabled() {
        return vibrationEnabled;
    }

    public static void startVibration(Context context, int intensity) {
        if (!vibrationEnabled) {
            return;
        }

        initialize(context);

        if (vibrator != null) {
            vibrator.cancel();
            // Reduced duration from 100ms to 50ms for lower latency
            long[] pattern = {0, 50};
            int[] amplitudes = {0, intensity};
            VibrationEffect effect = VibrationEffect.createWaveform(pattern, amplitudes, 0);
            vibrator.vibrate(effect);
        }
    }

    public static void stopVibration() {
        if (vibrator != null) {
            vibrator.cancel();
        }
    }
}
