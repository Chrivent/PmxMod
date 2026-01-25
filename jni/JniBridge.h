#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

    JNIEXPORT jlong JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCreate
      (JNIEnv*, jclass);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeDestroy
      (JNIEnv*, jclass, jlong);

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadPmx
      (JNIEnv*, jclass, jlong, jstring, jstring);

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeAddVmd
      (JNIEnv*, jclass, jlong, jstring);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeUpdate
      (JNIEnv*, jclass, jlong, jfloat, jfloat);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetVertexCount
      (JNIEnv*, jclass, jlong);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexCount
      (JNIEnv*, jclass, jlong);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexElementSize
      (JNIEnv*, jclass, jlong);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyIndices
      (JNIEnv*, jclass, jlong, jobject /*ByteBuffer*/);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyPositions
      (JNIEnv*, jclass, jlong, jobject /*FloatBuffer*/);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyNormals
      (JNIEnv*, jclass, jlong, jobject /*FloatBuffer*/);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyUVs
      (JNIEnv*, jclass, jlong, jobject /*FloatBuffer*/);

#ifdef __cplusplus
}
#endif
