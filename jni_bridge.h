#pragma once
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

    // 패키지/클래스 이름은 자바와 반드시 일치해야 합니다.
    // 예: Java_mod_Native_create → package: "mod", class: "Native"

    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCameraAnim_reset(JNIEnv*, jclass, jlong);

    JNIEXPORT jlong   JNICALL Java_net_chrivent_pmxstevemod_src_Native_create(JNIEnv*, jclass);
    JNIEXPORT void    JNICALL Java_net_chrivent_pmxstevemod_src_Native_destroy(JNIEnv*, jclass, jlong);

#ifdef __cplusplus
}
#endif