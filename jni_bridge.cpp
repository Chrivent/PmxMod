#include "jni_bridge.h"

#include "src/MMDModel.h"
#include "src/VMDCameraAnimation.h"
#include "src/VMDFile.h"

#include <memory>

struct MMDModelCore {
    std::unique_ptr<saba::MMDModel> mmdModel;
};

struct VMDAnimationCore {
    std::unique_ptr<saba::VMDAnimation> vmdAnim;
};

struct VMDCameraAnimationCore {
    std::unique_ptr<saba::VMDCameraAnimation> vmdCamAnim;
};

struct VMDFileCore {
    saba::VMDFile vmdFile;
};

static MMDModelCore* MMDModelFrom(jlong h) { return reinterpret_cast<MMDModelCore*>(h); }
static VMDAnimationCore* VMDAnimationFrom(jlong h) { return reinterpret_cast<VMDAnimationCore*>(h); }
static VMDCameraAnimationCore* VMDCameraAnimationFrom(jlong h) { return reinterpret_cast<VMDCameraAnimationCore*>(h); }
static VMDFileCore* VMDFileFrom(jlong h) { return reinterpret_cast<VMDFileCore*>(h); }

static void copy_buffer(JNIEnv* env, const void* src, jlong need, jobject dst) {
    void*  dstPtr = env->GetDirectBufferAddress(dst);
    jlong  cap    = env->GetDirectBufferCapacity(dst);
    if (!dstPtr || cap < need) return; // 필요 시 예외 처리 가능
    std::memcpy(dstPtr, src, (size_t)need);
}

static void copy_floats(JNIEnv* env, const float* src, int count, jobject dst) {
    void*  dstPtr = env->GetDirectBufferAddress(dst);
    jlong  cap = env->GetDirectBufferCapacity(dst);
    jlong  need = (jlong)count * 4; // float bytes
    if (!dstPtr || cap < 0 || cap < need) return;
    std::memcpy(dstPtr, src, (size_t)need);
}

// 객체 생성 및 해제

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_1new(JNIEnv*, jclass) {
    auto* core = new MMDModelCore();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_1delete(JNIEnv*, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h)) {
        c->mmdModel.reset();
        delete c;
    }
}

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDAnimationCore_1new(JNIEnv*, jclass) {
    auto* core = new VMDAnimationCore();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDAnimationCore_1delete(JNIEnv*, jclass, jlong h) {
    if (auto* c = VMDAnimationFrom(h)) {
        c->vmdAnim.reset();
        delete c;
    }
}

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDCameraAnimationCore_1new(JNIEnv*, jclass) {
    auto* core = new VMDCameraAnimationCore();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDCameraAnimationCore_1delete(JNIEnv*, jclass, jlong h) {
    if (auto* c = VMDCameraAnimationFrom(h)) {
        c->vmdCamAnim.reset();
        delete c;
    }
}

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDFileCore_1new(JNIEnv*, jclass) {
    auto* core = new VMDFileCore();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_VMDFileCore_1delete(JNIEnv*, jclass, jlong h) {
    if (auto* c = VMDFileFrom(h)) {
        delete c;
    }
}

// 함수 명령 전달

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetVertexCount(JNIEnv *, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h))
        return static_cast<jint>(c->mmdModel->m_positions.size());
    return 0;
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetIndexElementSize(JNIEnv *, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h))
        return static_cast<jint>(c->mmdModel->m_indexElementSize);
    return 0;
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetIndexCount(JNIEnv *, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h))
        return static_cast<jint>(c->mmdModel->m_indexCount);
    return 0;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetIndices(JNIEnv * env, jclass, jlong h, jobject dst) {
    if (auto* c = MMDModelFrom(h)) {
        const void* src = &c->mmdModel->m_indices[0];
        const jlong need = (jlong)c->mmdModel->m_indexElementSize * c->mmdModel->m_indexCount;
        copy_buffer(env, src, need, dst);
    }
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetMaterialCount(JNIEnv *, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h))
        return static_cast<jint>(c->mmdModel->m_materials.size());
    return 0;
}

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetMaterial(JNIEnv *, jclass, jlong h, jint idx) {
    if (auto* c = MMDModelFrom(h)) {
        return reinterpret_cast<jlong>(&c->mmdModel->m_materials[idx]);
    }
    return 0;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1BeginAnimation(JNIEnv*, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h)) {
        c->mmdModel->BeginAnimation();
    }
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1UpdateAllAnimation(JNIEnv*, jclass, jlong modelH, jlong vmdAnimH, jfloat vmdFrame, jfloat physicsElapsed) {
    if (auto* m = MMDModelFrom(modelH)) {
        if (auto* vmd = VMDAnimationFrom(vmdAnimH)) {
            m->mmdModel->UpdateAllAnimation(vmd->vmdAnim.get(), vmdFrame, physicsElapsed);
        }
    }
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1Update(JNIEnv*, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h)) {
        c->mmdModel->Update();
    }
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetUpdatePositions(JNIEnv* env, jclass, jlong h, jobject dst) {
    if (auto* c = MMDModelFrom(h)) {
        const auto& v = c->mmdModel->m_updatePositions;
        const void* src = v.data();
        const jlong need = (jlong)v.size() * (jlong)sizeof(glm::vec3);
        copy_buffer(env, src, need, dst);
    }
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetUpdateNormals(JNIEnv* env, jclass, jlong h, jobject dst) {
    if (auto* c = MMDModelFrom(h)) {
        const auto& v = c->mmdModel->m_updateNormals;
        const void* src = v.data();
        const jlong need = (jlong)v.size() * (jlong)sizeof(glm::vec3);
        copy_buffer(env, src, need, dst);
    }
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetUpdateUVs(JNIEnv* env, jclass, jlong h, jobject dst) {
    if (auto* c = MMDModelFrom(h)) {
        const auto& v = c->mmdModel->m_updateUVs;
        const void* src = v.data();
        const jlong need = (jlong)v.size() * (jlong)sizeof(glm::vec2);
        copy_buffer(env, src, need, dst);
    }
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetSubMeshCount(JNIEnv*, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h)) {
        return static_cast<jint>(c->mmdModel->m_subMeshes.size());
    }
    return 0;
}

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1GetSubMesh(JNIEnv*, jclass, jlong h, jint idx) {
    if (auto* c = MMDModelFrom(h)) {
        return reinterpret_cast<jlong>(&c->mmdModel->m_subMeshes[idx]);
    }
    return 0;
}

JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1Create(JNIEnv *, jclass, jlong camH, jlong vmdH) {
    if (auto* cam = VMDCameraAnimationFrom(camH))
    {
        if (auto* vf = VMDFileFrom(vmdH))
            return cam->vmdCamAnim->Create(vf->vmdFile);
    }
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1reset(JNIEnv *, jclass, jlong h) {
    if (auto* c = VMDCameraAnimationFrom(h))
        c->vmdCamAnim.reset();
}

JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTexture(JNIEnv* env, jclass, jlong h) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(h);
    return env->NewStringUTF(m->m_texture.c_str());
}

JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpTexture(JNIEnv* env, jclass, jlong h) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(h);
    return env->NewStringUTF(m->m_spTexture.c_str());
}

JNIEXPORT jstring JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonTexture(JNIEnv* env, jclass, jlong h) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(h);
    return env->NewStringUTF(m->m_toonTexture.c_str());
}

JNIEXPORT jfloat JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetAlpha(JNIEnv*, jclass, jlong h) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        return c->m_alpha;
    return 0;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetAmbient(JNIEnv* env, jclass, jlong h, jobject out3) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        copy_floats(env, &c->m_ambient[0], 3, out3);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetDiffuse(JNIEnv* env, jclass, jlong h, jobject out3) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        copy_floats(env, &c->m_diffuse[0], 3, out3);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpecular(JNIEnv* env, jclass, jlong h, jobject out3) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        copy_floats(env, &c->m_specular[0], 3, out3);
}

JNIEXPORT jfloat JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpecularPower(JNIEnv*, jclass, jlong h) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        return c->m_specularPower;
    return 0;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTextureMulFactor(JNIEnv* env, jclass, jlong h, jobject out4) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        copy_floats(env, &c->m_textureMulFactor[0], 4, out4);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetTextureAddFactor(JNIEnv* env, jclass, jlong h, jobject out4) {
    if (auto* c = reinterpret_cast<const saba::MMDMaterial*>(h))
        copy_floats(env, &c->m_textureAddFactor[0], 4, out4);
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDSubMesh_1GetBeginIndex(JNIEnv *, jclass, jlong h) {
    if (auto* c = reinterpret_cast<saba::MMDSubMesh*>(h)) {
        return c->m_beginIndex;
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDSubMesh_1GetVertexCount(JNIEnv *, jclass, jlong h) {
    if (auto* c = reinterpret_cast<saba::MMDSubMesh*>(h)) {
        return c->m_vertexCount;
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDSubMesh_1GetMaterialID(JNIEnv *, jclass, jlong h) {
    if (auto* c = reinterpret_cast<saba::MMDSubMesh*>(h)) {
        return c->m_materialID;
    }
    return 0;
}















JNIEXPORT jint JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpTextureMode
  (JNIEnv*, jclass, jlong matPtr) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    if (!m) return 0;
    // 0:none, 1:Mul, 2:Add
    switch (m->m_spTextureMode) {
        case saba::SphereTextureMode::Mul: return 1;
        case saba::SphereTextureMode::Add: return 2;
        default:                           return 0;
    }
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpMulFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    if (!m) return;
    copy_floats(env, &m->m_spTextureMulFactor[0], 4, out4);
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetSpAddFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    if (!m) return;
    copy_floats(env, &m->m_spTextureAddFactor[0], 4, out4);
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonMulFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    if (!m) return;
    copy_floats(env, &m->m_toonTextureMulFactor[0], 4, out4);
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetToonAddFactor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    if (!m) return;
    copy_floats(env, &m->m_toonTextureAddFactor[0], 4, out4);
}

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetBothFace
  (JNIEnv*, jclass, jlong matPtr) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    return (m && m->m_bothFace) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetEdgeFlag
  (JNIEnv*, jclass, jlong matPtr) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    return (m && m->m_edgeFlag) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetEdgeSize
  (JNIEnv*, jclass, jlong matPtr) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    return m ? (jfloat)m->m_edgeSize : 0.0f;
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetEdgeColor
  (JNIEnv* env, jclass, jlong matPtr, jobject out4) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    if (!m) return;
    copy_floats(env, &m->m_edgeColor[0], 4, out4);
}

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDMaterial_1GetGroundShadow
  (JNIEnv*, jclass, jlong matPtr) {
    const auto* m = reinterpret_cast<const saba::MMDMaterial*>(matPtr);
    return (m && m->m_groundShadow) ? JNI_TRUE : JNI_FALSE;
}

// --- MMDModel_Load ---
JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1Load
  (JNIEnv* env, jclass, jlong h, jstring jpmx, jstring jmmdDir)
{
    auto* c = MMDModelFrom(h);
    if (!c) return JNI_FALSE;

    const char* pmx    = env->GetStringUTFChars(jpmx, nullptr);
    const char* mmdDir = env->GetStringUTFChars(jmmdDir, nullptr);

    if (!c->mmdModel) c->mmdModel = std::make_unique<saba::MMDModel>();
    bool ok = c->mmdModel->Load(pmx, mmdDir);

    env->ReleaseStringUTFChars(jpmx, pmx);
    env->ReleaseStringUTFChars(jmmdDir, mmdDir);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_MMDModel_1InitializeAnimation
  (JNIEnv*, jclass, jlong h)
{
    auto* c = MMDModelFrom(h);
    if (c && c->mmdModel) c->mmdModel->InitializeAnimation();
}

// --- VMDAnimation ---
JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDAnimation_1SetModel
  (JNIEnv*, jclass, jlong vmdH, jlong modelH)
{
    auto* v = VMDAnimationFrom(vmdH);
    auto* m = MMDModelFrom(modelH);
    if (!v) return;
    if (!v->vmdAnim) v->vmdAnim = std::make_unique<saba::VMDAnimation>();
    // 공유 포인터로 연결
    if (m && m->mmdModel) {
        // 소유권 없는 shared_ptr (코어가 파괴되면 댕글링 위험!)
        std::shared_ptr<saba::MMDModel> alias(m->mmdModel.get(), [](saba::MMDModel*){});
        v->vmdAnim->m_model = std::move(alias);
    }
}

JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDAnimation_1Add
  (JNIEnv*, jclass, jlong vmdH, jlong fileH)
{
    auto* v = VMDAnimationFrom(vmdH);
    auto* f = VMDFileFrom(fileH);
    if (!v || !v->vmdAnim || !f) return JNI_FALSE;
    bool ok = v->vmdAnim->Add(f->vmdFile);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDAnimation_1SyncPhysics
  (JNIEnv*, jclass, jlong vmdH, jfloat t)
{
    auto* v = VMDAnimationFrom(vmdH);
    if (v && v->vmdAnim) v->vmdAnim->SyncPhysics(t);
}

// --- VMDFile ---
JNIEXPORT jboolean JNICALL
Java_net_chrivent_pmxstevemod_src_Native_VMDFile_1Read
  (JNIEnv* env, jclass, jlong fileH, jstring jpath)
{
    auto* f = VMDFileFrom(fileH);
    if (!f) return JNI_FALSE;
    const char* path = env->GetStringUTFChars(jpath, nullptr);
    bool ok = saba::ReadVMDFile(&f->vmdFile, path);
    env->ReleaseStringUTFChars(jpath, path);
    return ok ? JNI_TRUE : JNI_FALSE;
}
