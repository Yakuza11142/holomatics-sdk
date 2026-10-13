# ============================================================================
# TESSERACT SDK: MAXIMUM-SECURITY PROGUARD / R8 OBFUSCATION RULES
# ============================================================================

# 1. OPTIMIZATION & OBFUSCATION AGGRESSION
-optimizations !code/simplification/arithmetic,!code/simplification/cast,!field/*,!class/merging/*
-optimizationpasses 5
-allowaccessmodification
-repackageclasses 'com.tesseract.internal'
-useuniqueclassmembernames

# 2. JNI NATIVE BINDING PRESERVATION (CRITICAL)
# Prevents R8/ProGuard from renaming, stripping, or altering native method signatures 
# mapped in com.tesseract.ui.TessEngine and native C/C++ bridges.
-keepclassmembers class com.tesseract.ui.TessEngine {
    native <methods>;
    <fields> ...;
}

-keep public class com.tesseract.ui.TessEngine {
    public static void init();
    public native *** native*(...);
    <init>();
}

# Preserve the entire package namespace for structural Java-native interop
-keep class com.tesseract.ui.** {
    *;
}

# 3. STRIPPING UNNECESSARY LOGS & METADATA
# Removes all verbose debugging logs, print traces, and source file metadata in release builds
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int d(...);
}

-assumenosideeffects class java.lang.Throwable {
    public java.lang.String getMessage();
}

-renamesourcefileattribute TesseractSource
-keepattributes SourceFile,LineNumberTable,Signature,InnerClasses,EnclosingMethod

# 4. SERIALIZATION & DATA TRANSFER OBJECT (DTO) PROTECTION
# Ensures mapped structs and serialized map headers remain intact during code shrinking
-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    private static final java.io.ObjectStreamField[] serialPersistentFields;
    !static !transient <fields>;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}

# 5. KEEP ANNOTATIONS & REFLECTION INTERFACES
-keepattributes *Annotation*,Signature,Exception
