#pragma once
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

    // 패키지/클래스 이름은 자바와 반드시 일치해야 합니다.
    // 예: Java_mod_Native_create → package: "mod", class: "Native"

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_delete(JNIEnv*, jclass, jlong);

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDCameraAnimationCore_new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDCameraAnimationCore_delete(JNIEnv*, jclass, jlong);

    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDFileCore_new(JNIEnv*, jclass);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDFileCore_delete(JNIEnv*, jclass, jlong);

    // 함수 명령 전달

    JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_Create(JNIEnv *, jclass, jlong, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_reset(JNIEnv *, jclass, jlong);

#ifdef __cplusplus
}
#endif