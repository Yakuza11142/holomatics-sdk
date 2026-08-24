# Obfuscate all internal classes, methods, and fields into short names
-repackageclasses ''
-allowaccessmodification
-optimizationpasses 5

# Keep SWIG JNI entry points required for native interaction
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# Keep dynamic C-ABI exported classes
-keep class com.tesseract.ui.** { *; }
