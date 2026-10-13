/*
 * File: TessBridge.c
 * Target: Optional Android JNI Wrapper (Isolated via conditional compilation)
 */

#if defined(__ANDROID__)

#include <jni.h>
#include <android/log.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#define LOG_TAG "TessSDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ... Android JNI implementation methods here ...

#endif // __ANDROID__
