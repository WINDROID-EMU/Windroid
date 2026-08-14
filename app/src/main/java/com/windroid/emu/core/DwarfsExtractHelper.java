package com.windroid.emu.core;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Helper class that extracts the platform-appropriate {@code dwarfsextract} binary
 * from the app's assets and installs it to a writable directory, so it can be
 * executed at runtime to unpack DwarFS images (.dwarfs).
 *
 * <p>The binary is stored under {@code assets/bin/<abi>/dwarfsextract} and is
 * extracted once to {@code /data/data/com.windroid.emu/files/bin/dwarfsextract}.</p>
 */
public class DwarfsExtractHelper {

    private static final String TAG = "DwarfsExtractHelper";
    private static final String BIN_DIR = "bin";
    private static final String BINARY_NAME = "dwarfsextract";

    private static String cachedBinaryPath = null;

    /**
     * Returns the absolute path to the executable {@code dwarfsextract} binary,
     * copying it from assets if necessary.
     *
     * @param context Application context used to access assets and filesDir.
     * @return Absolute path to the binary, or {@code null} if it could not be installed.
     */
    public static synchronized String getBinaryPath(Context context) {
        if (cachedBinaryPath != null && new File(cachedBinaryPath).canExecute()) {
            return cachedBinaryPath;
        }

        String abi = getPrimaryAbi();
        String assetPath = BIN_DIR + "/" + abi + "/" + BINARY_NAME;

        File destDir = new File(context.getFilesDir(), BIN_DIR);
        if (!destDir.exists() && !destDir.mkdirs()) {
            Log.e(TAG, "Failed to create bin directory: " + destDir.getAbsolutePath());
            return null;
        }

        File destFile = new File(destDir, BINARY_NAME);

        // Only copy if not already present or if the asset is newer (check by re-copying)
        if (!destFile.exists()) {
            if (!copyAsset(context.getAssets(), assetPath, destFile)) {
                return null;
            }
        }

        if (!destFile.setExecutable(true, false)) {
            Log.w(TAG, "Could not set executable bit on " + destFile.getAbsolutePath());
        }

        if (!destFile.canExecute()) {
            Log.e(TAG, "Binary is not executable: " + destFile.getAbsolutePath());
            return null;
        }

        cachedBinaryPath = destFile.getAbsolutePath();
        Log.d(TAG, "dwarfsextract ready at: " + cachedBinaryPath);
        return cachedBinaryPath;
    }

    /**
     * Builds the extract command string to be passed to {@link ShellLoader#runCommand}.
     *
     * @param context    Application context.
     * @param inputPath  Path to the .dwarfs input file.
     * @param outputPath Directory where contents will be extracted.
     * @return Shell command string, or {@code null} if the binary is unavailable.
     */
    public static String buildExtractCommand(Context context, String inputPath, String outputPath) {
        String binary = getBinaryPath(context);
        if (binary == null) return null;
        return "'" + binary + "' -i '" + inputPath + "' -o '" + outputPath + "'";
    }

    /**
     * Builds a command to extract only a specific file pattern (e.g., {@code pkg-header}).
     */
    public static String buildExtractPatternCommand(Context context, String inputPath,
                                                     String outputPath, String pattern) {
        String binary = getBinaryPath(context);
        if (binary == null) return null;
        return "'" + binary + "' -i '" + inputPath + "' -o '" + outputPath
                + "' --pattern " + pattern + " 2>/dev/null";
    }

    // -------------------------------------------------------------------------

    private static String getPrimaryAbi() {
        // Normalise to what the NDK / asset folder naming uses
        String primaryAbi = Build.SUPPORTED_ABIS[0];
        // Only arm64-v8a and x86_64 are supported targets
        if (primaryAbi.equals("arm64-v8a") || primaryAbi.equals("x86_64")) {
            return primaryAbi;
        }
        // Fallback: arm64 devices report arm64-v8a first; x86 emulators report x86_64
        return "arm64-v8a";
    }

    private static boolean copyAsset(AssetManager assets, String assetPath, File dest) {
        try (InputStream in = assets.open(assetPath);
             FileOutputStream out = new FileOutputStream(dest)) {

            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            Log.d(TAG, "Copied asset " + assetPath + " -> " + dest.getAbsolutePath());
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy asset " + assetPath + ": " + e.getMessage());
            return false;
        }
    }
}
