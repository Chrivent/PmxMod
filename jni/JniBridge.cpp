#include "JniBridge.h"

#include "../src/Model.h"
#include "../src/Animation.h"
#include "../src/Reader.h"

struct PmxRuntime {
    std::shared_ptr<Model> model;
    std::unique_ptr<Animation> anim;
};

std::filesystem::path JStringToPath(JNIEnv* env, const jstring s) {
    if (!s)
        return {};
    const jchar* chars = env->GetStringChars(s, nullptr);
    const jsize len = env->GetStringLength(s);
    const std::wstring w(chars, chars + len);
    env->ReleaseStringChars(s, chars);
    return std::filesystem::path(w);
}

PmxRuntime* FromHandle(const jlong h) {
    return reinterpret_cast<PmxRuntime*>(h);
}

void CopyToDirectBuffer(JNIEnv* env, const jobject dstBuffer, const void* src, const size_t srcBytes) {
    if (!dstBuffer || !src)
        return;
    void* dst = env->GetDirectBufferAddress(dstBuffer);
    const jlong cap = env->GetDirectBufferCapacity(dstBuffer);
    if (!dst || cap <= 0)
        return;
    const size_t dstBytes = static_cast<size_t>(cap);
    const size_t n = srcBytes < dstBytes ? srcBytes : dstBytes;
    std::memcpy(dst, src, n);
}

extern "C" {
    JNIEXPORT jlong JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCreate(JNIEnv*, jclass) {
        auto* rt = new PmxRuntime{};
        rt->model = std::make_shared<Model>();
        rt->anim = std::make_unique<Animation>();
        rt->anim->m_model = rt->model;
        return reinterpret_cast<jlong>(rt);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeDestroy(JNIEnv*, jclass, jlong handle) {
        auto* rt = FromHandle(handle);
        if (!rt)
            return;
        if (rt->anim)
            rt->anim->Destroy();
        if (rt->model)
            rt->model->Destroy();
        delete rt;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadPmx(
        JNIEnv* env, jclass, jlong handle, jstring pmxPath, jstring dataDir) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return JNI_FALSE;
        const auto pmx = JStringToPath(env, pmxPath);
        const auto dir = JStringToPath(env, dataDir);
        const bool ok = rt->model->Load(pmx, dir);
        if (!ok)
            return JNI_FALSE;
        rt->model->InitializeAnimation();
        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeAddVmd(
        JNIEnv* env, jclass, const jlong handle, const jstring vmdPath) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->anim)
            return JNI_FALSE;
        VMDReader vmd;
        const auto path = JStringToPath(env, vmdPath);
        if (!vmd.ReadFile(path))
            return JNI_FALSE;
        const bool ok = rt->anim->Add(vmd);
        return ok ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeUpdate(
        JNIEnv*, jclass, const jlong handle, const jfloat frame, const jfloat physicsElapsed) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return;
        rt->model->BeginAnimation();
        rt->model->UpdateAllAnimation(rt->anim.get(), frame, physicsElapsed);
        rt->model->m_parallelUpdateCount = 1;
        rt->model->Update();
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetVertexCount(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return 0;
        return static_cast<jint>(rt->model->m_updatePositions.size());
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexCount(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return 0;
        return static_cast<jint>(rt->model->m_indexCount);
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexElementSize(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return 0;
        return static_cast<jint>(rt->model->m_indexElementSize);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyIndices(
        JNIEnv* env, jclass, const jlong handle, const jobject dstByteBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return;
        const void* src = rt->model->m_indices.data();
        const size_t bytes = rt->model->m_indices.size();
        CopyToDirectBuffer(env, dstByteBuffer, src, bytes);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyPositions(
        JNIEnv* env, jclass, const jlong handle, const jobject dstFloatBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return;
        const void* src = rt->model->m_updatePositions.data();
        const size_t bytes = rt->model->m_updatePositions.size() * sizeof(glm::vec3);
        CopyToDirectBuffer(env, dstFloatBuffer, src, bytes);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyNormals(
        JNIEnv* env, jclass, const jlong handle, const jobject dstFloatBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return;
        const void* src = rt->model->m_updateNormals.data();
        const size_t bytes = rt->model->m_updateNormals.size() * sizeof(glm::vec3);
        CopyToDirectBuffer(env, dstFloatBuffer, src, bytes);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyUVs(
        JNIEnv* env, jclass, const jlong handle, const jobject dstFloatBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model)
            return;
        const void* src = rt->model->m_updateUVs.data();
        const size_t bytes = rt->model->m_updateUVs.size() * sizeof(glm::vec2);
        CopyToDirectBuffer(env, dstFloatBuffer, src, bytes);
    }
}
