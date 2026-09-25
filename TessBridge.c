#include <jni.h>
#include <android/log.h>
#include <stdbool.h>
#include <pthread.h>

#define LOG_TAG "TessSDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global JVM tracker for background sensor/render thread attaching
static JavaVM* g_jvm = NULL;
static jobject g_engine_global_ref = NULL;
static pthread_mutex_t g_render_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef __cplusplus
extern "C" {
#endif

// Safe FFI Initialization hook executed during library load
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("JNI_OnLoad: Tesseract Native Engine loaded into JVM.");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeInit(JNIEnv* env, jobject thiz) {
    pthread_mutex_lock(&g_render_mutex);

    // Release any previously pinned instance to prevent reference leaks
    if (g_engine_global_ref != NULL) {
#ifdef __cplusplus
        env->DeleteGlobalRef(g_engine_global_ref);
#else
        (*env)->DeleteGlobalRef(env, g_engine_global_ref);
#endif
        g_engine_global_ref = NULL;
    }

    // Pin calling Java object globally to prevent Garbage Collection during async cycles
#ifdef __cplusplus
    g_engine_global_ref = env->NewGlobalRef(thiz);
#else
    g_engine_global_ref = (*env)->NewGlobalRef(env, thiz);
#endif

    LOGI("Tess Engine Core initialized successfully via JNI and pinned globally.");
    pthread_mutex_unlock(&g_render_mutex);
}

JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeRenderFrame(JNIEnv* env, jobject thiz) {
    // Thread safety latch: Non-blocking trylock skips dropped frame backpressure
    if (pthread_mutex_trylock(&g_render_mutex) != 0) {
        return; // Skip frame smoothly if the pipeline is busy
    }

    // Bare-metal rendering pipeline hooks execution point
    // e.g., tess_render_frame_direct();

    pthread_mutex_unlock(&g_render_mutex);
}

// Clean up routine called when the Android surface or activity context terminates
JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeShutdown(JNIEnv* env, jobject thiz) {
    pthread_mutex_lock(&g_render_mutex);

    if (g_engine_global_ref != NULL) {
#ifdef __cplusplus
        env->DeleteGlobalRef(g_engine_global_ref);
#else
        (*env)->DeleteGlobalRef(env, g_engine_global_ref);
#endif
        g_engine_global_ref = NULL;
    }

    LOGI("Tess Engine resources released cleanly from Android NDK runtime.");
    pthread_mutex_unlock(&g_render_mutex);
}

#ifdef __cplusplus
} // extern "C"
#endif
