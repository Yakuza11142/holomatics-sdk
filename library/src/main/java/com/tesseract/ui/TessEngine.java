package com.tesseract.ui;

public class TessEngine {
    static {
        System.loadLibrary("tessnative");
    }

    public static native void nativeInit();

    public static void init() {
        nativeInit();
    }
}
