# ====================================================================
# TessSDK Spatial Engine - Production ProGuard Rules
# ====================================================================

# 1. Protect Java-to-C Bridge Interfaces
# Forces ProGuard to preserve the exact package path and method names
# for native implementations inside your SDK namespace.
-keepclassmembers class com.holomatics.sdk.** {
    native <methods>;
}

# 2. Prevent Universal Obfuscation of Native Entry Points
# Ensures any class declaring a native method keeps its name intact,
# preventing runtime 'java.lang.UnsatisfiedLinkError' crashes.
-keepclasseswithmembernames class * {
    native <methods>;
}

# 3. Suppress Warnings From Core Lint and AAPT2 Dependencies
# Prevents build compilation failures caused by non-critical warning flags
# in the standard build toolchain.
-dontwarn com.android.tools.lint.**
-dontwarn com.android.tools.build.**

# 4. Maximize Optimization Passes
# Allows the optimizer to run aggressively multiple times to shrink 
# your background Java framework wrappers down as small as possible.
-optimizationpasses 5
