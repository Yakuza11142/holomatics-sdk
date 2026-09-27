# ----------------------------------------------------------------------------
# 🔐 AGGRESSIVE CORE OBFUSCATION & MAXIMUM SHRINKING
# ----------------------------------------------------------------------------

# Repackage all classes into a single root package for maximum obfuscation
-repackageclasses ''
-allowaccessmodification

# Increase optimization depth for deeper dead-code elimination
-optimizationpasses 5

# Strip out verbose debug and verbose logging calls completely from release bytecode
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int d(...);
}

# ----------------------------------------------------------------------------
# 🔗 TARGETED NATIVE JNI BOUNDARY PRESERVATION
# ----------------------------------------------------------------------------

# FIX: Scope native checks strictly to your namespace instead of scanning every third-party jar/aar
-keepclasseswithmembernames class com.tesseract.** {
    native <methods>;
}
-includedescriptorclasses

# Tighten UI preservation: Keep class definitions and constructors for layout inflation, 
# but allow ProGuard to strip unused private fields and internal helper methods.
-keep public class com.tesseract.ui.** {
    <init>(...);
    public void set*(...);
    public *** get*();
}

# Trim non-essential attributes (removed broad wildcard annotations to save space)
-keepattributes Signature, InnerClasses, EnclosingMethod, AnnotationDefault
