#pragma once

#include <jni.h>

extern "C" {
    JNIEXPORT jlong JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCreate(JNIEnv*, jclass);
    JNIEXPORT void  JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeDestroy(JNIEnv*, jclass, jlong);

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadPmx(JNIEnv*, jclass, jlong, jstring, jstring);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeAddVmd(JNIEnv*, jclass, jlong, jstring);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeStartVmdBlend(JNIEnv*, jclass, jlong, jstring, jfloat);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeStartDefaultPoseBlend(JNIEnv*, jclass, jlong, jfloat);
    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMotionMaxFrame(JNIEnv*, jclass, jlong);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadCameraVmd(JNIEnv*, jclass, jlong, jstring);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeClearCamera(JNIEnv*, jclass, jlong);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetCameraState(JNIEnv*, jclass, jlong, jfloat, jobject);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativePlayMusicLoop(JNIEnv*, jclass, jlong, jstring, jboolean);
    JNIEXPORT jdouble JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMusicLengthSec(JNIEnv*, jclass, jlong);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeSetMusicVolume(JNIEnv*, jclass, jlong, jfloat);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeStopMusic(JNIEnv*, jclass, jlong);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMusicTimes(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeSyncPhysics(JNIEnv*, jclass, jlong, jfloat);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeUpdate(JNIEnv*, jclass, jlong, jfloat, jfloat);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeSetBoneRotationAdditive(JNIEnv*, jclass, jlong, jstring, jfloat, jfloat, jfloat);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeHasBone(JNIEnv*, jclass, jlong, jstring);

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
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshBeginIndex(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshIndexCount(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshMaterialId(JNIEnv*, jclass, jlong, jint);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialCount(JNIEnv*, jclass, jlong);

    JNIEXPORT jint   JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialDiffuseRGBA(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialAlpha(JNIEnv*, jclass, jlong, jint);

    JNIEXPORT void   JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialAmbient(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT void   JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSpecular(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSpecularPower(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialBothFaceByMaterial(JNIEnv*, jclass, jlong, jint);

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialTexturePath(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereTexturePath(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jint    JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereMode(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialToonTexturePath(JNIEnv*, jclass, jlong, jint);

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialTexMulFactor(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialTexAddFactor(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereMulFactor(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereAddFactor(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialToonMulFactor(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialToonAddFactor(JNIEnv*, jclass, jlong, jint, jobject);

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialEdgeFlag(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialEdgeSize(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialEdgeColor(JNIEnv*, jclass, jlong, jint, jobject);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialGroundShadow(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialShadowCaster(JNIEnv*, jclass, jlong, jint);
    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialShadowReceiver(JNIEnv*, jclass, jlong, jint);
}
