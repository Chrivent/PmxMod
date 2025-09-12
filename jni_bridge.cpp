#include "jni_bridge.h"

#include "src/MMDModel.h"
#include "base/Path.h"

#include <memory>

struct Core {
    std::unique_ptr<saba::MMDModel> model;
    // vmd anim 포인터, 시간 누적값, 오디오 컨텍스트 등 필요한 것들
    float animTime = 0.f;
};

static Core* from(jlong h) { return reinterpret_cast<Core*>(h); }

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_create(JNIEnv*, jclass) {
    auto* core = new Core();
    core->model = std::make_unique<saba::MMDModel>();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_destroy(JNIEnv*, jclass, jlong h) {
    if (auto* c = from(h)) { c->model.reset(); delete c; }
}

JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_src_Native_loadModel(
    JNIEnv* env, jclass, jlong h, jstring pmxPath, jobjectArray vmdPaths, jfloat scale)
{
    auto* c = from(h); if (!c) return JNI_FALSE;

    // UTF-8 문자열 얻기
    const char* pmx = env->GetStringUTFChars(pmxPath, nullptr);
    const bool ok = c->model->Load(pmx, /*mmdDataDir*/ saba::PathUtil::GetDirectoryName(pmx));
    env->ReleaseStringUTFChars(pmxPath, pmx);
    if (!ok) return JNI_FALSE;

    c->model->InitializeAnimation();
    // VMD 파일들 읽어서 애니 세팅(생략) ...
    // scale은 c->model 쪽에서 적용하거나 자바 렌더 쪽에서 행렬로 적용

    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_step(JNIEnv*, jclass, jlong h, jfloat dt) {
    auto* c = from(h); if (!c) return;
    c->animTime += dt;
    const float vmdFrame = c->animTime * 30.0f;
    c->model->UpdateAllAnimation(/*vmdAnim*/nullptr, vmdFrame, /*physicsElapsed*/dt);
}

// ==== 버퍼 쿼리 예시 (DirectByteBuffer) ====
JNIEXPORT jint JNICALL Java_net_chrivent_pmxstevemod_src_Native_getVertexCount(JNIEnv*, jclass, jlong h) {
    auto* c = from(h); return c ? (jint)c->model->m_positions.size() : 0;
}

JNIEXPORT jobject JNICALL Java_net_chrivent_pmxstevemod_src_Native_getPositions(JNIEnv* env, jclass, jlong h) {
    auto* c = from(h); if (!c) return nullptr;
    auto& v = c->model->m_positions;                 // std::vector<glm::vec3>
    return env->NewDirectByteBuffer(v.data(), v.size() * sizeof(v[0]));
}

JNIEXPORT jobject JNICALL Java_mod_chrivent_pmxstevemod_src_Native_getIndices(JNIEnv* env, jclass, jlong h) {
    auto* c = from(h); if (!c) return nullptr;
    auto& idx = c->model->m_indices;                       // std::vector<char>
    return env->NewDirectByteBuffer(idx.data(), idx.size());
}