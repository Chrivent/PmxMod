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
