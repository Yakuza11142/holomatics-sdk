#include <jni.h>
#include <android/log.h>
#include <stdbool.h>
#include <pthread.h>

#define LOG_TAG "TessSDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global JVM tracker for background sensor/render thread attaching
static JavaVM* g_jvm = nullptr;
static jobject g_engine_global_ref = nullptr;
static pthread_mutex_t g_render_mutex = PTHREAD_MUTEX_INITIALIZER;

// Protect names from C++ compiler mangling
extern "C" {

// Safe FFI Initialization hook executed automatically during library load
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("Tess SDK Shared Library Loaded. JavaVM cached successfully.");
    return JNI_VERSION_1_6;
}

// System Shutdown callback executed during library unload
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    g_jvm = nullptr;
    LOGI("Tess SDK Shared Library Unloaded.");
}

JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeInit(JNIEnv* env, jobject thiz) {
    pthread_mutex_lock(&g_render_mutex);

    // Protect the calling Java instance from garbage collection during async loops
    if (g_engine_global_ref != nullptr) {
        env->DeleteGlobalRef(g_engine_global_ref);
    }
    g_engine_global_ref = env->NewGlobalRef(thiz);

    LOGI("Tess Engine Core initialized successfully via JNI and pinned globally.");
    pthread_mutex_unlock(&g_render_mutex);
}

JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeRenderFrame(JNIEnv* env, jobject thiz) {
    // Thread safety latch to keep asynchronous camera frames from crashing under heavy frame-rates
    if (pthread_mutex_trylock(&g_render_mutex) != 0) {
        return; // Skip frame smoothly if the layout pipeline is currently busy
    }

    // Helper block to safely support calls originating from native background threads
    JNIEnv* current_env = env;
    bool is_thread_attached = false;
    
    if (g_jvm != nullptr) {
        jint env_status = g_jvm->GetEnv((void**)&current_env, JNI_VERSION_1_6);
        if (env_status == JNI_EDETACHED) {
            if (g_jvm->AttachCurrentThread(&current_env, nullptr) == JNI_OK) {
                is_thread_attached = true;
            } else {
                LOGE("Critical: Failed to attach asynchronous render thread to JVM context.");
                pthread_mutex_unlock(&g_render_mutex);
                return;
            }
        }
    }

    // -------------------------------------------------------------
    // TODO: Hook your bare-metal rendering loops safely here:
    // tess_render_frame_direct();
    // -------------------------------------------------------------

    // Clean up thread environment binding cleanly if attached internally
    if (is_thread_attached && g_jvm != nullptr) {
        g_jvm->DetachCurrentThread();
    }

    pthread_mutex_unlock(&g_render_mutex);
}

// Clean up routine called when the Android surface or application layer terminates
JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeShutdown(JNIEnv* env, jobject thiz) {
    pthread_mutex_lock(&g_render_mutex);
    
    if (g_engine_global_ref != nullptr) {
        env->DeleteGlobalRef(g_engine_global_ref);
        g_engine_global_ref = nullptr;
    }
    
    LOGI("Tess Engine resources released cleanly from Android NDK runtime.");
    pthread_mutex_unlock(&g_render_mutex);
}

} // extern "C"
