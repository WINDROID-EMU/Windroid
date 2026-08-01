package com.windroid.emu;

// This interface is used by utility on termux side.
interface ICmdEntryInterface {
    void windowChanged(in Surface surface);
    Surface getSurface();
    ParcelFileDescriptor getXConnection();
    ParcelFileDescriptor getLogcatOutput();
}