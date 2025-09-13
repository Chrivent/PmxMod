#pragma once
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

    // 패키지/클래스 이름은 자바와 반드시 일치해야 합니다.
    // 예: Java_mod_Native_create → package: "mod", class: "Native"

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_1new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_1delete(JNIEnv*, jclass, jlong);

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDAnimationCore_1new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDAnimationCore_1delete(JNIEnv*, jclass, jlong);

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDCameraAnimationCore_1new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDCameraAnimationCore_1delete(JNIEnv*, jclass, jlong);

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDFileCore_1new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDFileCore_1delete(JNIEnv*, jclass, jlong);

    // 함수 명령 전달

    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetVertexCount(JNIEnv *, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetIndexElementSize(JNIEnv *, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetIndexCount(JNIEnv *, jclass, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetIndices(JNIEnv *, jclass, jlong, jobject);
    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetMaterialCount(JNIEnv *, jclass, jlong);
    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetMaterial(JNIEnv *, jclass, jlong, jint);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1BeginAnimation(JNIEnv*, jclass, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1UpdateAllAnimation(JNIEnv*, jclass, jlong, jlong, jfloat, jfloat);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1Update(JNIEnv*, jclass, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetUpdatePositions(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetUpdateNormals(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetUpdateUVs(JNIEnv* env, jclass, jlong, jobject);

    JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1Create(JNIEnv *, jclass, jlong, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1reset(JNIEnv *, jclass, jlong);

    JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTexture(JNIEnv *, jclass, jlong);
    JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpTexture(JNIEnv *, jclass, jlong);
    JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonTexture(JNIEnv *, jclass, jlong);

#ifdef __cplusplus
}
#endif