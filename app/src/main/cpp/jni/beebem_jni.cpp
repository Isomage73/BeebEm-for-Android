#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define LOG_TAG "BeebEm"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeInit(
        JNIEnv* /* env */, jobject /* obj */,
        jobject /* assetManager */, jstring /* dataDir */) {
    LOGI("BeebEm core initialised");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeShutdown(
        JNIEnv* /* env */, jobject /* obj */) {
    LOGI("BeebEm core shutdown");
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeReset(
        JNIEnv* /* env */, jobject /* obj */, jboolean hardReset) {
    LOGI("BeebEm reset (hard=%d)", (int)hardReset);
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeRunFrame(
        JNIEnv* /* env */, jobject /* obj */,
        jobject /* frameBuffer */, jintArray /* widthOut */, jintArray /* heightOut */) {
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeKeyDown(
        JNIEnv* /* env */, jobject /* obj */, jint /* beebRow */, jint /* beebCol */) {
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeKeyUp(
        JNIEnv* /* env */, jobject /* obj */, jint /* beebRow */, jint /* beebCol */) {
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeBreakKey(
        JNIEnv* /* env */, jobject /* obj */, jboolean /* shift */) {
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeMountDisc(
        JNIEnv* /* env */, jobject /* obj */,
        jint /* drive */, jstring /* path */, jboolean /* writeProtect */) {
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeEjectDisc(
        JNIEnv* /* env */, jobject /* obj */, jint /* drive */) {
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeIsDiscModified(
        JNIEnv* /* env */, jobject /* obj */, jint /* drive */) {
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeFlushDisc(
        JNIEnv* /* env */, jobject /* obj */, jint /* drive */) {
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeSaveState(
        JNIEnv* /* env */, jobject /* obj */, jstring /* path */) {
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeLoadState(
        JNIEnv* /* env */, jobject /* obj */, jstring /* path */) {
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeSetSoundEnabled(
        JNIEnv* /* env */, jobject /* obj */, jboolean /* enabled */) {
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeSetSpeedLimit(
        JNIEnv* /* env */, jobject /* obj */, jint /* percent */) {
}

} // extern "C"
