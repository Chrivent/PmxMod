#include "JniBridge.h"

#include "../src/Model.h"
#include "../src/Animation.h"
#include "../src/Reader.h"
#include "../src/Util.h"

struct PmxRuntime {
    std::shared_ptr<Model> model;
    std::unique_ptr<Animation> anim;
};

std::filesystem::path JStringToPath(JNIEnv* env, _jstring* s) {
    if (!s) return {};
    const jchar* chars = env->GetStringChars(s, nullptr);
    const jsize len = env->GetStringLength(s);
    const std::wstring w(chars, chars + len);
    env->ReleaseStringChars(s, chars);
    return std::filesystem::path{w};
}

static jstring PathToJStringUtf8(JNIEnv* env, const std::filesystem::path& p) {
    const std::wstring w = p.wstring();
    const std::string utf8 = Util::WStringToUtf8(w);
    if (utf8.empty()) return nullptr;
    return env->NewStringUTF(utf8.c_str());
}

PmxRuntime* FromHandle(const jlong h) {
    return reinterpret_cast<PmxRuntime*>(h);
}

void CopyToDirectBuffer(JNIEnv* env, _jobject* dstBuffer, const void* src, const size_t srcBytes) {
    if (!dstBuffer || !src) return;
    void* dst = env->GetDirectBufferAddress(dstBuffer);
    const jlong cap = env->GetDirectBufferCapacity(dstBuffer);
    if (!dst || cap <= 0) return;
    const auto dstBytes = static_cast<size_t>(cap);
    const size_t n = srcBytes < dstBytes ? srcBytes : dstBytes;
    std::memcpy(dst, src, n);
}

static jint PackRGBA8(const float r, const float g, const float b, const float a) {
    auto to8 = [](float x) -> jint {
        x = std::clamp(x, 0.0f, 1.0f);
        return std::lround(x * 255.0f);
    };
    const jint R = to8(r);
    const jint G = to8(g);
    const jint B = to8(b);
    const jint A = to8(a);
    return R << 24 | G << 16 | B << 8 | A;
}

static void WriteVec3(JNIEnv* env, _jobject* buf, const glm::vec3& v) {
    if (!buf) return;
    auto* out = static_cast<float*>(env->GetDirectBufferAddress(buf));
    const jlong cap = env->GetDirectBufferCapacity(buf);
    if (!out || cap < static_cast<jlong>(3 * sizeof(float))) return;
    out[0] = v.x; out[1] = v.y; out[2] = v.z;
}

static void WriteVec4(JNIEnv* env, _jobject* buf, const glm::vec4& v) {
    if (!buf) return;
    auto* out = static_cast<float*>(env->GetDirectBufferAddress(buf));
    const jlong cap = env->GetDirectBufferCapacity(buf);
    if (!out || cap < static_cast<jlong>(4 * sizeof(float))) return;
    out[0] = v.x; out[1] = v.y; out[2] = v.z; out[3] = v.w;
}

static jint SphereModeToInt(const SphereMode m) {
    if (m == SphereMode::Mul) return 1;
    if (m == SphereMode::Add) return 2;
    return 0;
}

static const Material* GetMaterialByIndex(const Model* model, const int mi) {
    if (!model) return nullptr;
    const auto& mats = model->m_materials;
    if (mi < 0 || mi >= static_cast<int>(mats.size())) return nullptr;
    return &mats[mi];
}

extern "C" {
    JNIEXPORT jlong JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCreate(JNIEnv*, jclass) {
        auto* rt = new PmxRuntime{};
        rt->model = std::make_shared<Model>();
        rt->anim = std::make_unique<Animation>();
        rt->anim->m_model = rt->model;
        return reinterpret_cast<jlong>(rt);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeDestroy(JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt) return;
        if (rt->anim) rt->anim->Destroy();
        if (rt->model) rt->model->Destroy();
        delete rt;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadPmx(
        JNIEnv* env, jclass, const jlong handle, _jstring* pmxPath, _jstring* dataDir) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return JNI_FALSE;
        const auto pmx = JStringToPath(env, pmxPath);
        const auto dir = JStringToPath(env, dataDir);
        const bool ok = rt->model->Load(pmx, dir);
        if (!ok) return JNI_FALSE;
        rt->model->InitializeAnimation();
        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeAddVmd(
        JNIEnv* env, jclass, const jlong handle, _jstring* vmdPath) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->anim) return JNI_FALSE;
        VMDReader vmd;
        const auto path = JStringToPath(env, vmdPath);
        if (!vmd.ReadFile(path)) return JNI_FALSE;
        const bool ok = rt->anim->Add(vmd);
        return ok ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeUpdate(
        JNIEnv*, jclass, const jlong handle, const jfloat frame, const jfloat physicsElapsed) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return;
        rt->model->BeginAnimation();
        rt->model->UpdateAllAnimation(rt->anim.get(), frame, physicsElapsed);
        rt->model->m_parallelUpdateCount = 1;
        rt->model->Update();
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetModelName(
        JNIEnv* env, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return nullptr;
        return env->NewStringUTF(rt->model->m_modelName.c_str());
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetEnglishModelName(
        JNIEnv* env, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return nullptr;
        return env->NewStringUTF(rt->model->m_englishModelName.c_str());
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetComment(
        JNIEnv* env, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return nullptr;
        return env->NewStringUTF(rt->model->m_comment.c_str());
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetEnglishComment(
        JNIEnv* env, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return nullptr;
        return env->NewStringUTF(rt->model->m_englishComment.c_str());
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetVertexCount(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return 0;
        return static_cast<jint>(rt->model->m_updatePositions.size());
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexCount(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return 0;
        return static_cast<jint>(rt->model->m_indexCount);
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetIndexElementSize(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return 0;
        return static_cast<jint>(rt->model->m_indexElementSize);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyIndices(
        JNIEnv* env, jclass, const jlong handle, _jobject* dstByteBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return;
        const void* src = rt->model->m_indices.data();
        const size_t bytes = rt->model->m_indices.size();
        CopyToDirectBuffer(env, dstByteBuffer, src, bytes);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyPositions(
        JNIEnv* env, jclass, const jlong handle, _jobject* dstByteBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model || !dstByteBuffer) return;
        const auto out = static_cast<float*>(env->GetDirectBufferAddress(dstByteBuffer));
        const jlong capBytes = env->GetDirectBufferCapacity(dstByteBuffer);
        if (!out || capBytes <= 0) return;
        const size_t maxFloats = static_cast<size_t>(capBytes) / sizeof(float);
        const auto& src = rt->model->m_updatePositions;
        const size_t n = min(src.size(), maxFloats / 3);
        for (size_t i = 0; i < n; i++) {
            const auto& v = src[i];
            out[i * 3 + 0] = v.x;
            out[i * 3 + 1] = v.y;
            out[i * 3 + 2] = v.z;
        }
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyNormals(
        JNIEnv* env, jclass, const jlong handle, _jobject* dstByteBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model || !dstByteBuffer) return;
        const auto out = static_cast<float*>(env->GetDirectBufferAddress(dstByteBuffer));
        const jlong capBytes = env->GetDirectBufferCapacity(dstByteBuffer);
        if (!out || capBytes <= 0) return;
        const size_t maxFloats = static_cast<size_t>(capBytes) / sizeof(float);
        const auto& src = rt->model->m_updateNormals;
        const size_t n = min(src.size(), maxFloats / 3);
        for (size_t i = 0; i < n; i++) {
            const auto& v = src[i];
            out[i * 3 + 0] = v.x;
            out[i * 3 + 1] = v.y;
            out[i * 3 + 2] = v.z;
        }
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeCopyUVs(
        JNIEnv* env, jclass, const jlong handle, _jobject* dstByteBuffer) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model || !dstByteBuffer) return;
        const auto out = static_cast<float*>(env->GetDirectBufferAddress(dstByteBuffer));
        const jlong capBytes = env->GetDirectBufferCapacity(dstByteBuffer);
        if (!out || capBytes <= 0) return;
        const size_t maxFloats = static_cast<size_t>(capBytes) / sizeof(float);
        const auto& src = rt->model->m_updateUVs;
        const size_t n = min(src.size(), maxFloats / 2);
        for (size_t i = 0; i < n; i++) {
            const auto& v = src[i];
            out[i * 2 + 0] = v.x;
            out[i * 2 + 1] = 1.0f - v.y;
        }
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshCount(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return 0;
        return static_cast<jint>(rt->model->m_subMeshes.size());
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshBeginIndex(
        JNIEnv*, jclass, const jlong handle, const jint m) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return 0;
        const int si = static_cast<int>(m);
        const auto& sub = rt->model->m_subMeshes;
        if (si < 0 || si >= static_cast<int>(sub.size())) return 0;
        return sub[si].m_beginIndex;
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshIndexCount(
        JNIEnv*, jclass, const jlong handle, const jint m) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return 0;
        const int si = static_cast<int>(m);
        const auto& sub = rt->model->m_subMeshes;
        if (si < 0 || si >= static_cast<int>(sub.size())) return 0;
        return sub[si].m_indexCount;
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetSubmeshMaterialId(
        JNIEnv*, jclass, const jlong h, const jint s) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return -1;
        const auto& subs = rt->model->m_subMeshes;
        if (s < 0 || s >= static_cast<jint>(subs.size())) return -1;
        return subs[s].m_materialID;
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialCount(
        JNIEnv*, jclass, const jlong h) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return 0;
        return static_cast<jint>(rt->model->m_materials.size());
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialDiffuseRGBA(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return static_cast<jint>(0xFFFFFFFF);
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return static_cast<jint>(0xFFFFFFFF);
        const auto& d = mat->m_diffuse;
        return PackRGBA8(d.x, d.y, d.z, d.w);
    }

    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialAlpha(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return 1.0f;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return 1.0f;
        return mat->m_diffuse.a;
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialAmbient(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst3f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const auto& mats = rt->model->m_materials;
        if (m < 0 || m >= static_cast<jint>(mats.size())) return;
        WriteVec3(env, dst3f, mats[m].m_ambient);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSpecular(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst3f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec3(env, dst3f, mat->m_specular);
    }

    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSpecularPower(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return 0.0f;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return 0.0f;
        return mat->m_specularPower;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialBothFaceByMaterial(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return JNI_FALSE;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return JNI_FALSE;
        return mat->m_bothFace ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialTexturePath(
        JNIEnv* env, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return nullptr;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return nullptr;
        return PathToJStringUtf8(env, mat->m_texture);
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereTexturePath(
        JNIEnv* env, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return nullptr;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return nullptr;
        return PathToJStringUtf8(env, mat->m_spTexture);
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereMode(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return 0;
        const auto& mats = rt->model->m_materials;
        if (m < 0 || m >= static_cast<jint>(mats.size())) return 0;
        return SphereModeToInt(mats[m].m_spTextureMode);
    }

    JNIEXPORT jstring JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialToonTexturePath(
        JNIEnv* env, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return nullptr;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return nullptr;
        return PathToJStringUtf8(env, mat->m_toonTexture);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialTexMulFactor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_textureMulFactor);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialTexAddFactor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_textureAddFactor);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereMulFactor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_spTextureMulFactor);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialSphereAddFactor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_spTextureAddFactor);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialToonMulFactor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_toonTextureMulFactor);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialToonAddFactor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_toonTextureAddFactor);
    }

    JNIEXPORT jint JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialEdgeFlag(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return 0;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return 0;
        return (jint)mat->m_edgeFlag;
    }

    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialEdgeSize(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return 0.0f;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return 0.0f;
        return mat->m_edgeSize;
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialEdgeColor(
        JNIEnv* env, jclass, const jlong h, const jint m, _jobject* dst4f) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return;
        WriteVec4(env, dst4f, mat->m_edgeColor);
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialGroundShadow(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return JNI_FALSE;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return JNI_FALSE;
        return mat->m_groundShadow ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialShadowCaster(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return JNI_FALSE;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return JNI_FALSE;
        return mat->m_shadowCaster ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMaterialShadowReceiver(
        JNIEnv*, jclass, const jlong h, const jint m) {
        const auto* rt = FromHandle(h);
        if (!rt || !rt->model) return JNI_FALSE;
        const Material* mat = GetMaterialByIndex(rt->model.get(), static_cast<int>(m));
        if (!mat) return JNI_FALSE;
        return mat->m_shadowReceiver ? JNI_TRUE : JNI_FALSE;
    }
}
