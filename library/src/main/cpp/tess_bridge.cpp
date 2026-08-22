#include <jni.h>
#include <android/log.h>

#define LOG_TAG "TessSDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_tesseract_ui_TessEngine_nativeInit(JNIEnv* env, jobject thiz) {
    LOGI("Tess Engine Core initialized successfully via JNI.");
}
