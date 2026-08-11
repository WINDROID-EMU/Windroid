package com.windroid.emu.core;

public class FEXCoreWrapper {
    static {
        System.loadLibrary("windroid");
    }

    /**
     * Set the path to FEXInterpreter executable
     * @param path Path to FEXInterpreter
     * @return true if path set successfully, false otherwise
     */
    public native boolean setFEXInterpreterPath(String path);

    /**
     * Set the path to the rootfs directory
     * @param path Path to rootfs directory
     * @return true if path set successfully, false otherwise
     */
    public native boolean setRootFSPath(String path);

    /**
     * Initialize FEXCore for x86_64 emulation on ARM64 Android
     * @return true if initialization successful, false otherwise
     */
    public native boolean initialize();

    /**
     * Shutdown FEXCore and cleanup resources
     */
    public native void shutdown();

    /**
     * Load an ELF executable for emulation
     * @param elfPath Path to the ELF executable
     * @return true if loading successful, false otherwise
     */
    public native boolean loadELF(String elfPath);

    /**
     * Run the loaded ELF executable
     * @param argc Argument count
     * @param argv Argument array
     * @return Exit code
     */
    public native int run(int argc, String[] argv);

    /**
     * Get FEXCore version string
     * @return Version string
     */
    public native String getVersion();

    private static FEXCoreWrapper instance;

    public static synchronized FEXCoreWrapper getInstance() {
        if (instance == null) {
            instance = new FEXCoreWrapper();
        }
        return instance;
    }
}
