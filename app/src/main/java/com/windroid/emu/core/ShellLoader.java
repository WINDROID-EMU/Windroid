package com.windroid.emu.core;

import androidx.annotation.Keep;

@Keep
public class ShellLoader {
    private static final StringBuilder sessionLogs = new StringBuilder();

    static {
        System.loadLibrary("micewine");
    }

    public interface LogCallback {
        void appendLogs(String text);
    }

    public static native void cleanup();
    public static native void connectOutput(LogCallback callback);
    public static native int runCommand(String command, boolean log);
    public static native String runCommandWithOutput(String command, boolean strErrLog);

    public static synchronized void addToSessionLogs(String text) {
        sessionLogs.append(text);
        if (sessionLogs.length() > 500000) { // Limit buffer size to ~0.5MB
            sessionLogs.delete(0, sessionLogs.length() - 400000);
        }
    }

    public static synchronized String getSessionLogs() {
        return sessionLogs.toString();
    }
}