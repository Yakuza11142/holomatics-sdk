package com.tesseract.ui;

public class TessEngine {
    static {
        // Loads your compiled bare-metal library binary (e.g., libTessSDK.so)
        System.loadLibrary("TessSDK");
    }

    public native void nativeInit();
    public native void nativeRenderFrame();
    public native void nativeShutdown();
}
