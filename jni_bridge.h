#pragma once
#include <jni.h>

#ifdef __cplusplus
namespace saba {
  struct MMDMaterial;
}

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
    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetSubMeshCount(JNIEnv*, jclass, jlong);
    JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetSubMesh(JNIEnv*, jclass, jlong, jint);

    JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1Create(JNIEnv *, jclass, jlong, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1reset(JNIEnv *, jclass, jlong);

    JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTexture(JNIEnv *, jclass, jlong);
    JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpTexture(JNIEnv *, jclass, jlong);
    JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonTexture(JNIEnv *, jclass, jlong);
    JNIEXPORT jfloat JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetAlpha(JNIEnv*, jclass, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetAmbient(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetDiffuse(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpecular(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT jfloat JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpecularPower(JNIEnv*, jclass, jlong);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTextureMulFactor(JNIEnv*, jclass, jlong, jobject);
    JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTextureAddFactor(JNIEnv*, jclass, jlong, jobject);

    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDSubMesh_1GetBeginIndex(JNIEnv *, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDSubMesh_1GetVertexCount(JNIEnv *, jclass, jlong);
    JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDSubMesh_1GetMaterialID(JNIEnv *, jclass, jlong);



// ===== Material props =====
JNIEXPORT jint JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpTextureMode
  (JNIEnv*, jclass, jlong matPtr);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpMulFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpAddFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonMulFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonAddFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4);

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetBothFace
  (JNIEnv*, jclass, jlong matPtr);

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetEdgeFlag
  (JNIEnv*, jclass, jlong matPtr);

JNIEXPORT jfloat JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetEdgeSize
  (JNIEnv*, jclass, jlong matPtr);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetEdgeColor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4);

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetGroundShadow
  (JNIEnv*, jclass, jlong matPtr);

  // --- MMDModel_Load ---
JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1Load
  (JNIEnv* env, jclass, jlong h, jstring jpmx, jstring jmmdDir);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1InitializeAnimation
  (JNIEnv*, jclass, jlong h);

// --- VMDAnimation ---
JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDAnimation_1SetModel
  (JNIEnv*, jclass, jlong vmdH, jlong modelH);

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDAnimation_1Add
  (JNIEnv*, jclass, jlong vmdH, jlong fileH);

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDAnimation_1SyncPhysics
  (JNIEnv*, jclass, jlong vmdH, jfloat t);

// --- VMDFile ---
JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDFile_1Read
  (JNIEnv* env, jclass, jlong fileH, jstring jpath);

#ifdef __cplusplus
}
#endif