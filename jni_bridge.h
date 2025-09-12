#pragma once
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

    // 패키지/클래스 이름은 자바와 반드시 일치해야 합니다.
    // 예: Java_mod_Native_create → package: "mod", class: "Native"

    JNIEXPORT jlong   JNICALL Java_net_chrivent_pmxstevemod_Native_create(JNIEnv*, jclass);
    JNIEXPORT void    JNICALL Java_net_chrivent_pmxstevemod_Native_destroy(JNIEnv*, jclass, jlong);

    JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_Native_loadModel(JNIEnv*, jclass, jlong,
        jstring pmxPath, jobjectArray vmdPaths, jfloat scale);
    JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_Native_setMusic(JNIEnv*, jclass, jlong,
        jstring musicPath, jboolean sync);

    JNIEXPORT void    JNICALL Java_net_chrivent_pmxstevemod_Native_step(JNIEnv*, jclass, jlong, jfloat);

    JNIEXPORT jint    JNICALL Java_net_chrivent_pmxstevemod_Native_getVertexCount(JNIEnv*, jclass, jlong);
    JNIEXPORT jint    JNICALL Java_net_chrivent_pmxstevemod_Native_getIndexCount(JNIEnv*, jclass, jlong);
    JNIEXPORT jint    JNICALL Java_net_chrivent_pmxstevemod_Native_getIndexElementSize(JNIEnv*, jclass, jlong);

    JNIEXPORT jobject JNICALL Java_net_chrivent_pmxstevemod_Native_getPositions(JNIEnv*, jclass, jlong);
    JNIEXPORT jobject JNICALL Java_net_chrivent_pmxstevemod_Native_getNormals    (JNIEnv*, jclass, jlong);
    JNIEXPORT jobject JNICALL Java_net_chrivent_pmxstevemod_Native_getUVs            (JNIEnv*, jclass, jlong);
    JNIEXPORT jobject JNICALL Java_mod_Native_getIndices  (JNIEnv*, jclass, jlong);

    JNIEXPORT jobject JNICALL Java_net_chrivent_pmxstevemod_Native_getSubMeshes(JNIEnv*, jclass, jlong);
    JNIEXPORT jobject JNICALL Java_net_chrivent_pmxstevemod_Native_getMaterials(JNIEnv*, jclass, jlong);

#ifdef __cplusplus
}
#endif