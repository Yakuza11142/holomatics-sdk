# ----------------------------------------------------------------------------
# 🔐 HARDENED CORE OBFUSCATION & PERFORMANCE TUNING
# ----------------------------------------------------------------------------

# Obfuscate all internal classes, methods, and fields into short, scrambled names
-repackageclasses ''
-allowaccessmodification

# Safe optimization threshold for complex native multi-threaded asynchronous runtimes
-optimizationpasses 2

# ----------------------------------------------------------------------------
# 🔗 JNI & NATIVE PLATFORM BOUNDARY PRESERVATION
# ----------------------------------------------------------------------------

# FIX: Corrected syntax to explicitly preserve native methods and signature descriptors
-keepclasseswithmembernames class * {
    native <methods>;
}
-includedescriptorclasses

# Keep all class structures and native mappings inside your UI wrapper namespace
-keep class com.tesseract.ui.** { *; }

# Prevent the compiler from stripping or scrambling vital JVM tracking parameters
-keepattributes Signature, InnerClasses, EnclosingMethod, AnnotationDefault, *Annotation*
