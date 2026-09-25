package com.tesseract.demo;

import android.os.Bundle;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import com.tesseract.ui.TessEngine;

/**
 * Main entry point activity for managing the native Tesseract Engine lifecycle.
 */
public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Optional: Set your layout view if needed
        // setContentView(R.layout.activity_main);

        initializeTessEngine();
    }

    /**
     * Handles safe, isolated initialization of the native spatial runtime.
     */
    private void initializeTessEngine() {
        try {
            // Safely initialize the core engine runtime
            TessEngine.init();
            Log.i(TAG, "TessEngine initialized successfully.");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native C/C++ libraries for TessEngine", e);
        } catch (Exception e) {
            Log.e(TAG, "Unexpected error during TessEngine initialization", e);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // Clean up native allocations to prevent memory leaks if required by API
        try {
            // TessEngine.destroy(); // Uncomment if TessEngine exposes a tear-down method
        } catch (Exception e) {
            Log.e(TAG, "Error during TessEngine teardown", e);
        }
    }
}
