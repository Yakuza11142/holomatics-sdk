package com.tesseract.demo;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import com.tesseract.ui.TessEngine;

public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Initialize the Tess Engine Core
        TessEngine.init();
    }
}
