plugins {
    id 'com.android.library'
}

android {
    // 1. NAMESPACE BINDING: Must match your native JNI C++ function bindings exactly
    namespace 'com.tesseract.ui'
    compileSdk 34

    defaultConfig {
        // 2. KERNEL REQUIREMENT: Set to API 29 because the engine uses Linux 'memfd_create'
        minSdk 29
        targetSdk 34

        ndk {
            // 3. UNIVERSAL ARCHITECTURES: Compiles binaries for modern 64-bit phones, 
            // legacy 32-bit devices, and PC-based testing emulators.
            abiFilters 'arm64-v8a', 'x86_64', 'armeabi-v7a', 'x86'
        }

        externalNativeBuild {
            cmake {
                // 4. PERFORMANCE TUNING: Applies aggressive -O3 loop optimizations 
                // and -flto (Link-Time Optimization) across both C math and C++ modules.
                cFlags "-O3 -flto"
                cppFlags "-std=c++17 -O3 -flto"
                arguments "-DANDROID_STL=c++_shared"
            }
        }
    }

    buildTypes {
        release {
            // 5. MAXIMUM SHRINKING: Enables total Java minification and dead-code stripping
            minifyEnabled true
            
            // Note: 'shrinkResources' is omitted here because it is not supported 
            // in standalone Android library modules—only in application modules.
            
            // References your aggressive obfuscation rule file
            proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
        }
    }

    externalNativeBuild {
        cmake {
            path "CMakeLists.txt"
            version "3.22.1"
        }
    }
}

dependencies {
    // Heavy local tools have been permanently stripped.
    // Add lightweight, standard runtime dependencies here only if needed.
}
