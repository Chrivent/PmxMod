#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

    JNIEXPORT jlong JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCreate(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeDestroy(JNIEnv*, jclass, jlong);

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadPmx(JNIEnv*, jclass, jlong, jstring, jstring);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeAddVmd(JNIEnv*, jclass, jlong, jstring);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeUpdate(JNIEnv*, jclass, jlong, jfloat, jfloat);

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetModelName(JNIEnv* env, jclass, jlong handle);
    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetEnglishModelName(JNIEnv* env, jclass, jlong handle);
    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetComment(JNIEnv* env, jclass, jlong handle);
    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetEnglishComment(JNIEnv* env, jclass, jlong handle);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetVertexCount(JNIEnv*, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexCount(JNIEnv*, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexElementSize(JNIEnv*, jclass, jlong);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyIndices(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyPositions(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyNormals(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyUVs(JNIEnv*, jclass, jlong, jobject);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshCount(JNIEnv*, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshIndexStart(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshVertexCount(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshTexturePath(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshAlpha(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshDiffuseRGBA(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshBothFace(JNIEnv*, jclass, jlong, jint);

#ifdef __cplusplus
}
#endif
