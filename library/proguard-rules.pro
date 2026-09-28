# ====================================================================
# TessSDK Spatial Engine - MAX AGGRESSIVE PROGUARD CONFIGURATION
# ====================================================================

# 1. Core JNI Security Guard (DO NOT TOUCH)
# We MUST keep the names of classes that contain native methods so the .so file can link to them.
-keepclasseswithmembernames class * {
    native <methods>;
}
-keep class com.tesseract.ui.** {
    native <methods>;
}

# 2. Maximum Obfuscation & Repackaging Engines
# Flattens your entire package hierarchy into a single, confusing root folder
-repackageclasses 'a'
-allowaccessmodification

# Use aggressive, short variable and class name recycling (a, b, c...)
-dontusemixedcaseclassnames

# 3. Aggressive Code Stripping
# Permanently delete all debugging metadata, source file names, and line numbers
-renamesourcefileattribute SourceFile
-keepattributes !SourceFile,!LineNumberTable,*Annotation*,Signature,EnclosingMethod,InnerClasses

# 4. Maximum Optimization Passes
# Run the dead-code stripping engine 10 times consecutively instead of 5
-optimizationpasses 10

# 5. Dangerous/Maximum Optimizations (Enabled)
# Allows ProGuard to merge classes and make radical optimizations to reduce space
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int d(...);
}

# 6. Toolchain Warning Suppression
-dontwarn com.android.tools.lint.**
-dontwarn com.android.tools.build.**
