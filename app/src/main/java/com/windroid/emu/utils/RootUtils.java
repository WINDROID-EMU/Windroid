package com.windroid.emu.utils;

import android.util.Log;

import static com.windroid.emu.core.ShellLoader.runCommand;
import static com.windroid.emu.core.ShellLoader.runCommandWithOutput;

public class RootUtils {
    private static final String TAG = "RootUtils";
    private static String[] originalCpuGovernors;
    private static String originalGpuGovernor;
    private static boolean originalSelinuxPermissive;

    /**
     * Checks if root access is available.
     * @return true if root is available.
     */
    public static boolean isRootAvailable() {
        String testRoot = runCommandWithOutput("su -c 'echo test' 2>/dev/null", false);
        return testRoot != null && testRoot.contains("test");
    }

    /**
     * Applies performance mode by setting CPU and GPU governors to 'performance'.
     */
    public static void applyPerformanceMode() {
        if (!isRootAvailable()) {
            Log.w(TAG, "Root not available, cannot apply performance mode");
            return;
        }

        Log.i(TAG, "Applying performance mode...");

        // CPU
        int cpuCount = Runtime.getRuntime().availableProcessors();
        originalCpuGovernors = new String[cpuCount];

        for (int i = 0; i < cpuCount; i++) {
            String path = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_governor";
            originalCpuGovernors[i] = runCommandWithOutput("su -c 'cat " + path + "'", false);
            if (originalCpuGovernors[i] != null) {
                originalCpuGovernors[i] = originalCpuGovernors[i].trim();
                runCommand("su -c 'echo performance > " + path + "'", false);
            }
        }

        // GPU (Adreno)
        String adrenoPath = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
        originalGpuGovernor = runCommandWithOutput("su -c 'cat " + adrenoPath + "'", false);
        if (originalGpuGovernor != null) {
            originalGpuGovernor = originalGpuGovernor.trim();
            runCommand("su -c 'echo performance > " + adrenoPath + "'", false);
        } else {
            // Try another common path
            adrenoPath = "/sys/class/kgsl/kgsl-3d0/governor";
            originalGpuGovernor = runCommandWithOutput("su -c 'cat " + adrenoPath + "'", false);
            if (originalGpuGovernor != null) {
                originalGpuGovernor = originalGpuGovernor.trim();
                runCommand("su -c 'echo performance > " + adrenoPath + "'", false);
            }
        }

        optimizeIOScheduler();
    }

    /**
     * Restores default governor settings.
     */
    public static void restoreDefaultMode() {
        if (!isRootAvailable()) return;

        Log.i(TAG, "Restoring default mode...");

        // CPU
        if (originalCpuGovernors != null) {
            for (int i = 0; i < originalCpuGovernors.length; i++) {
                if (originalCpuGovernors[i] != null) {
                    runCommand("su -c 'echo " + originalCpuGovernors[i] + " > /sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_governor'", false);
                }
            }
        } else {
            // Fallback to schedutil or interactive
            for (int i = 0; i < Runtime.getRuntime().availableProcessors(); i++) {
                runCommand("su -c 'echo schedutil > /sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_governor' 2>/dev/null", false);
                runCommand("su -c 'echo interactive > /sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_governor' 2>/dev/null", false);
            }
        }

        // GPU
        if (originalGpuGovernor != null) {
            String adrenoPath = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
            runCommand("su -c 'echo " + originalGpuGovernor + " > " + adrenoPath + "' 2>/dev/null", false);
            adrenoPath = "/sys/class/kgsl/kgsl-3d0/governor";
            runCommand("su -c 'echo " + originalGpuGovernor + " > " + adrenoPath + "' 2>/dev/null", false);
        } else {
            // Fallback
            runCommand("su -c 'echo msm-adreno-tz > /sys/class/kgsl/kgsl-3d0/devfreq/governor' 2>/dev/null", false);
            runCommand("su -c 'echo msm-adreno-tz > /sys/class/kgsl/kgsl-3d0/governor' 2>/dev/null", false);
        }

        // Reset I/O Scheduler
        runCommand("su -c 'echo cfq > /sys/block/mmcblk0/queue/scheduler' 2>/dev/null", false);
        runCommand("su -c 'echo mq-deadline > /sys/block/mmcblk0/queue/scheduler' 2>/dev/null", false);
    }

    /**
     * Otimiza processos do Wine/Box64 para máxima prioridade.
     */
    public static void optimizeWineProcesses() {
        if (!isRootAvailable()) return;

        Log.i(TAG, "Optimizing process priorities...");

        // Aumenta a prioridade do wineserver e processos box64
        String pids = runCommandWithOutput("su -c 'pgrep -f \"wineserver|box64\"'", false);
        if (pids != null && !pids.isEmpty()) {
            for (String pid : pids.split("\n")) {
                pid = pid.trim();
                if (!pid.isEmpty()) {
                    // CPU Priority (Niceness)
                    runCommand("su -c 'renice -n -20 -p " + pid + "'", false);
                    // I/O Priority
                    runCommand("su -c 'ionice -c 1 -n 0 -p " + pid + "'", false);
                    // OOM Protection
                    runCommand("su -c 'echo -1000 > /proc/" + pid + "/oom_score_adj'", false);
                }
            }
        }
    }

    /**
     * Melhora o scheduler de I/O para reduzir latência.
     */
    public static void optimizeIOScheduler() {
        if (!isRootAvailable()) return;

        Log.i(TAG, "Optimizing I/O scheduler...");
        // Tenta definir para deadline ou noop que costumam ser melhores para emulação
        runCommand("su -c 'echo deadline > /sys/block/mmcblk0/queue/scheduler' 2>/dev/null", false);
        runCommand("su -c 'echo noop > /sys/block/mmcblk0/queue/scheduler' 2>/dev/null", false);
        runCommand("su -c 'echo 0 > /sys/block/mmcblk0/queue/add_random' 2>/dev/null", false);
        runCommand("su -c 'echo 1024 > /sys/block/mmcblk0/queue/read_ahead_kb' 2>/dev/null", false);
    }

    /**
     * Verifica se o tmpfs já está montado.
     */
    public static boolean isTmpfsMounted() {
        String mounts = runCommandWithOutput("su -c 'cat /proc/mounts'", false);
        return mounts != null && mounts.contains("/data/data/com.windroid.emu/files/usr/tmp");
    }

    /**
     * Monta um tmpfs na pasta temporária da esync para evitar I/O na memória Flash.
     */
    public static void mountTmpfs() {
        if (!isRootAvailable() || isTmpfsMounted()) return;
        
        Log.i(TAG, "Mounting tmpfs for esync...");
        runCommand("su -c 'mkdir -p /data/data/com.windroid.emu/files/usr/tmp'", false);
        
        // Try mounting without SELinux context first (most compatible)
        runCommand("su -c 'mount -t tmpfs -o size=256M,mode=1777 tmpfs /data/data/com.windroid.emu/files/usr/tmp'", false);
        
        // If that failed, try with generic SELinux context as fallback
        if (!isTmpfsMounted()) {
            runCommand("su -c 'mount -t tmpfs -o size=256M,mode=1777,context=u:object_r:app_data_file:s0 tmpfs /data/data/com.windroid.emu/files/usr/tmp'", false);
        }

        if (isTmpfsMounted()) {
            Log.i(TAG, "tmpfs successfully mounted in RAM.");
        } else {
            Log.e(TAG, "Failed to mount tmpfs. Esync will use physical storage.");
        }
    }

    /**
     * Desmonta o tmpfs quando o Wine finaliza.
     */
    public static void umountTmpfs() {
        if (!isRootAvailable()) return;
        
        Log.i(TAG, "Unmounting tmpfs...");
        runCommand("su -c 'umount /data/data/com.windroid.emu/files/usr/tmp'", false);
    }

    /**
     * Sets SELinux to permissive mode or enforcing mode.
     * @param permissive true to set to permissive (0), false to set to enforcing (1).
     */
    public static void setSelinuxPermissive(boolean permissive) {
        if (!isRootAvailable()) return;

        Log.i(TAG, "Setting SELinux to " + (permissive ? "permissive" : "enforcing") + "...");
        runCommand("su -c 'setenforce " + (permissive ? "0" : "1") + "'", false);
    }

    /**
     * Applies SELinux permissive mode and saves the original state.
     */
    public static void applySelinuxPermissive() {
        if (!isRootAvailable()) return;
        originalSelinuxPermissive = isSelinuxPermissive();
        if (!originalSelinuxPermissive) {
            setSelinuxPermissive(true);
        }
    }

    /**
     * Restores the original SELinux state.
     */
    public static void restoreSelinuxMode() {
        if (!isRootAvailable()) return;
        if (!originalSelinuxPermissive) {
            setSelinuxPermissive(false);
        }
    }

    /**
     * Checks if SELinux is currently permissive.
     * @return true if permissive, false if enforcing or unknown.
     */
    public static boolean isSelinuxPermissive() {
        if (!isRootAvailable()) return false;

        String output = runCommandWithOutput("su -c 'getenforce'", false);
        return output != null && output.trim().equalsIgnoreCase("Permissive");
    }
}
