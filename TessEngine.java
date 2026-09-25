package com.tesseract.ui;

import android.util.Log;
import androidx.annotation.Keep;

/**
 * Thread-safe Java JNI binding wrapper for the native Tesseract Spatial Engine runtime.
 */
@Keep
public class TessEngine {

    private static final String TAG = "TessEngine";
    private static final String LIBRARY_NAME = "TessSDK";

    private static volatile boolean isLibraryLoaded = false;

    static {
        loadNativeLibrary();
    }

    /**
     * Safely loads the underlying C/C++/Rust shared native library (.so).
     */
    private static synchronized void loadNativeLibrary() {
        if (!isLibraryLoaded) {
            try {
                System.loadLibrary(LIBRARY_NAME);
                isLibraryLoaded = true;
                Log.i(TAG, "Native library 'lib" + LIBRARY_NAME + ".so' loaded successfully.");
            } catch (UnsatisfiedLinkError e) {
                Log.e(TAG, "Critical failure: Could not load native library 'lib" + LIBRARY_NAME + ".so'", e);
            } catch (Exception e) {
                Log.e(TAG, "Unexpected error loading native library 'lib" + LIBRARY_NAME + ".so'", e);
            }
        }
    }

    /**
     * Public static initializer called by MainActivity or application entry points.
     */
    public static void init() {
        if (!isLibraryLoaded) {
            loadNativeLibrary();
        }
    }

    /**
     * Checks if the native binary library was successfully loaded.
     */
    public static boolean isLoaded() {
        return isLibraryLoaded;
    }

    // Native Interface Methods matching C/C++ JNI Symbol Declarations

    /**
     * Initializes the native render pipeline and pins the Java instance reference in NDK memory.
     */
    public native void nativeInit();

    /**
     * Signals the native rasterizer to draw a frame. Safe to call on render loop threads.
     */
    public native void nativeRenderFrame();

    /**
     * Cleanly tears down native allocations and releases pinned JVM references.
     */
    public native void nativeShutdown();
}
