#include <jni.h>
#include <android/log.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#define LOG_TAG "TessSDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Opaque context structure matching cross-platform implementations
typedef struct TesseractContext {
    void* internal_pipeline;
    float cumulative_time;
} TesseractContext;

// Global JVM tracker for background sensor/render thread attaching
static JavaVM* g_jvm = NULL;
static jobject g_engine_global_ref = NULL;
static pthread_mutex_t g_render_mutex = PTHREAD_MUTEX_INITIALIZER;
static TesseractContext* g_native_ctx = NULL;

// ----------------------------------------------------------------------------
// BARE-METAL BINDINGS (Exposed for Go, Flutter, C#, and Swift)
// ----------------------------------------------------------------------------

TesseractContext* tess_create(void) {
    TesseractContext* ctx = (TesseractContext*)malloc(sizeof(TesseractContext));
    if (ctx != NULL) {
        ctx->internal_pipeline = NULL;
        ctx->cumulative_time = 0.0f;
    }
    return ctx;
}

int tess_process_frame(TesseractContext* ctx, float deltaTime) {
    if (ctx == NULL) return -1;
    ctx->cumulative_time += deltaTime;
    // Execute low-level core matrix math here
    return 0;
}

void tess_destroy(TesseractContext* ctx) {
    if (ctx != NULL) {
        free(ctx);
    }
}

// ----------------------------------------------------------------------------
// HELPER METHODS FOR SAFE JNI ASYNC THREAD ATTACHMENT
// ----------------------------------------------------------------------------

static JNIEnv* get_jni_env(int* needs_detach) {
    JNIEnv* env = NULL;
    *needs_detach = 0;
    
    if (g_jvm == NULL) return NULL;
    
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        // Native thread is loose. Explicitly attach it to prevent an OS abort crash.
        #ifdef __ANDROID__
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
        #else
        res = (*g_jvm)->AttachCurrentThread(g_jvm, (void**)&env, NULL);
        #endif
        if (res == JNI_OK) {
            *needs_detach = 1;
        } else {
            LOGE("Critical Error: Failed to attach asynchronous native thread to JVM.");
            return NULL;
        }
    }
    return env;
}

// ----------------------------------------------------------------------------
// JNI INTERFACE IMPLEMENTATIONS (Exposed for Android Java)
// ----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

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
        (*env)->DeleteGlobalRef(env, g_engine_global_ref);
        g_engine_global_ref = NULL;
    }

    if (g_native_ctx != NULL) {
        tess_destroy(g_native_ctx);
    }

    // Pin calling Java object globally to shield it from Garbage Collection sweep passes
    g_engine_global_ref = (*env)->NewGlobalRef(env, thiz);
    
    // Allocate the bare-metal processing target frame context
    g_native_ctx = tess_create();

    LOGI("Tess Engine Core initialized successfully via JNI and pinned globally.");
    pthread_mutex_unlock(&g_render_mutex);
}

JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeRenderFrame(JNIEnv* env, jobject thiz) {
    // Thread safety latch: Non-blocking trylock skips dropped frame backpressure smoothly
    if (pthread_mutex_trylock(&g_render_mutex) != 0) {
        return; 
    }

    if (g_native_ctx != NULL) {
        // Execute unified processing logic (Passing a simulated 60FPS frame timing step)
        tess_process_frame(g_native_ctx, 0.01667f);
    }

    pthread_mutex_unlock(&g_render_mutex);
}

JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeShutdown(JNIEnv* env, jobject thiz) {
    pthread_mutex_lock(&g_render_mutex);

    if (g_engine_global_ref != NULL) {
        (*env)->DeleteGlobalRef(env, g_engine_global_ref);
        g_engine_global_ref = NULL;
    }

    if (g_native_ctx != NULL) {
        tess_destroy(g_native_ctx);
        g_native_ctx = NULL;
    }

    LOGI("Tess Engine resources released cleanly from Android NDK runtime.");
    pthread_mutex_unlock(&g_render_mutex);
}

#ifdef __cplusplus
}
#endif
