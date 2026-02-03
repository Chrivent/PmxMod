#include "JniBridge.h"

#include "../src/Model.h"
#include "../src/Animation.h"
#include "../src/Reader.h"
#include "../src/Sound.h"
#include "../src/Util.h"

#include <glm/gtc/type_ptr.hpp>
#include <ranges>

struct PmxRuntime {
    std::shared_ptr<Model> model;
    std::unique_ptr<Animation> anim;
    std::unique_ptr<Animation> blendAnim;
    std::unique_ptr<CameraAnimation> cameraAnim;
    float blendElapsed = 0.0f;
    float blendDuration = 0.0f;
    bool blending = false;
    bool blendToDefault = false;
    float motionMaxFrame = 0.0f;
    Sound music;
};

std::filesystem::path JStringToPath(JNIEnv* env, _jstring* s) {
    if (!s) return {};
    const jchar* chars = env->GetStringChars(s, nullptr);
    const jsize len = env->GetStringLength(s);
    const std::wstring w(chars, chars + len);
    env->ReleaseStringChars(s, chars);
    return std::filesystem::path{w};
}

static std::string JStringToUtf8(JNIEnv* env, _jstring* s) {
    const jchar* chars = env->GetStringChars(s, nullptr);
    const jsize len = env->GetStringLength(s);
    const std::wstring w(chars, chars + len);
    env->ReleaseStringChars(s, chars);
    return Util::WStringToUtf8(w);
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

static std::unique_ptr<Animation> CreateAnimation(const std::shared_ptr<Model>& model) {
    auto anim = std::make_unique<Animation>();
    anim->m_model = model;
    return anim;
}

static float GetVmdMaxFrame(const VMDReader& vmd) {
    uint32_t maxFrame = 0;
    for (const auto& motion : vmd.m_motions)
        maxFrame = max(maxFrame, motion.m_frame);
    for (const auto& morph : vmd.m_morphs)
        maxFrame = max(maxFrame, morph.m_frame);
    for (const auto& ik : vmd.m_iks)
        maxFrame = max(maxFrame, ik.m_frame);
    return static_cast<float>(maxFrame);
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

static void WriteMat4(JNIEnv* env, _jobject* buf, const glm::mat4& m) {
    auto* out = static_cast<float*>(env->GetDirectBufferAddress(buf));
    const jlong cap = env->GetDirectBufferCapacity(buf);
    if (!out || cap < static_cast<jlong>(16 * sizeof(float))) return;
    const float* src = glm::value_ptr(m);
    for (int i = 0; i < 16; ++i)
        out[i] = src[i];
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
        rt->anim = CreateAnimation(rt->model);
        return reinterpret_cast<jlong>(rt);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeDestroy(JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt) return;
        if (rt->anim) rt->anim->Destroy();
        if (rt->blendAnim) rt->blendAnim->Destroy();
        if (rt->model) rt->model->Destroy();
        delete rt;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadPmx(
        JNIEnv* env, jclass, const jlong handle, _jstring* pmxPath, _jstring* dataDir) {
        auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return JNI_FALSE;
        const auto pmx = JStringToPath(env, pmxPath);
        const auto dir = JStringToPath(env, dataDir);
        const bool ok = rt->model->Load(pmx, dir);
        if (!ok) return JNI_FALSE;
        rt->model->InitializeAnimation();
        rt->anim = CreateAnimation(rt->model);
        rt->blendAnim.reset();
        rt->cameraAnim.reset();
        rt->blendElapsed = 0.0f;
        rt->blendDuration = 0.0f;
        rt->blending = false;
        rt->blendToDefault = false;
        rt->motionMaxFrame = 0.0f;
        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeAddVmd(
        JNIEnv* env, jclass, const jlong handle, _jstring* vmdPath) {
        auto* rt = FromHandle(handle);
        if (!rt || !rt->anim) return JNI_FALSE;
        rt->anim = CreateAnimation(rt->model);
        rt->blendAnim.reset();
        rt->blendElapsed = 0.0f;
        rt->blendDuration = 0.0f;
        rt->blending = false;
        rt->blendToDefault = false;
        rt->motionMaxFrame = 0.0f;
        VMDReader vmd;
        const auto path = JStringToPath(env, vmdPath);
        if (!vmd.ReadFile(path)) return JNI_FALSE;
        rt->motionMaxFrame = GetVmdMaxFrame(vmd);
        const bool ok = rt->anim->Add(vmd);
        return ok ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeStartVmdBlend(
        JNIEnv* env, jclass, const jlong handle, _jstring* vmdPath, const jfloat blendSeconds) {
        auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return JNI_FALSE;
        VMDReader vmd;
        const auto path = JStringToPath(env, vmdPath);
        if (!vmd.ReadFile(path)) return JNI_FALSE;
        rt->motionMaxFrame = GetVmdMaxFrame(vmd);
        auto nextAnim = CreateAnimation(rt->model);
        if (!nextAnim->Add(vmd)) return JNI_FALSE;
        if (blendSeconds <= 0.0f) {
            rt->anim = std::move(nextAnim);
            rt->blendAnim.reset();
            rt->blendElapsed = 0.0f;
            rt->blendDuration = 0.0f;
            rt->blending = false;
            rt->blendToDefault = false;
            rt->motionMaxFrame = GetVmdMaxFrame(vmd);
            return JNI_TRUE;
        }
        rt->model->SaveBaseAnimation();
        rt->blendAnim = std::move(nextAnim);
        rt->blendElapsed = 0.0f;
        rt->blendDuration = blendSeconds;
        rt->blending = true;
        rt->blendToDefault = false;
        rt->motionMaxFrame = GetVmdMaxFrame(vmd);
        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeStartDefaultPoseBlend(
        JNIEnv*, jclass, const jlong handle, const jfloat blendSeconds) {
        auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return JNI_FALSE;
        if (blendSeconds <= 0.0f) {
            rt->blendAnim.reset();
            rt->blendElapsed = 0.0f;
            rt->blendDuration = 0.0f;
            rt->blending = false;
            rt->blendToDefault = false;
            rt->motionMaxFrame = 0.0f;
            for (const auto& node : rt->model->m_nodes) {
                node->m_animTranslate = glm::vec3(0);
                node->m_animRotate = glm::quat(1, 0, 0, 0);
            }
            for (const auto& morph : rt->model->m_morphs)
                morph->m_weight = 0.0f;
            for (const auto& ikSolver : rt->model->m_ikSolvers)
                ikSolver->m_enable = true;
            return JNI_TRUE;
        }
        rt->model->SaveBaseAnimation();
        rt->blendAnim.reset();
        rt->blendElapsed = 0.0f;
        rt->blendDuration = blendSeconds;
        rt->blending = true;
        rt->blendToDefault = true;
        rt->motionMaxFrame = 0.0f;
        return JNI_TRUE;
    }

    JNIEXPORT jfloat JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMotionMaxFrame(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt) return 0.0f;
        return rt->motionMaxFrame;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeLoadCameraVmd(
        JNIEnv* env, jclass, const jlong handle, _jstring* vmdPath) {
        auto* rt = FromHandle(handle);
        if (!rt) return JNI_FALSE;
        VMDReader vmd;
        const auto path = JStringToPath(env, vmdPath);
        if (!vmd.ReadFile(path)) return JNI_FALSE;
        auto cam = std::make_unique<CameraAnimation>();
        if (!cam->Create(vmd)) return JNI_FALSE;
        rt->cameraAnim = std::move(cam);
        return JNI_TRUE;
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeClearCamera(
        JNIEnv*, jclass, const jlong handle) {
        auto* rt = FromHandle(handle);
        if (!rt) return;
        rt->cameraAnim.reset();
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetCameraState(
        JNIEnv* env, jclass, const jlong handle, const jfloat frame, _jobject* dst8f) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->cameraAnim || !dst8f) return JNI_FALSE;
        auto* out = static_cast<float*>(env->GetDirectBufferAddress(dst8f));
        const jlong cap = env->GetDirectBufferCapacity(dst8f);
        if (!out || cap < static_cast<jlong>(8 * sizeof(float))) return JNI_FALSE;
        rt->cameraAnim->Evaluate(frame);
        const auto& [m_interest, m_rotate,
            m_distance, m_fov] = rt->cameraAnim->m_camera;
        out[0] = m_interest.x;
        out[1] = m_interest.y;
        out[2] = m_interest.z;
        out[3] = m_rotate.x;
        out[4] = m_rotate.y;
        out[5] = m_rotate.z;
        out[6] = m_distance;
        out[7] = m_fov;
        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativePlayMusicLoop(
        JNIEnv* env, jclass, const jlong handle, _jstring* musicPath, const jboolean loop) {
        auto* rt = FromHandle(handle);
        if (!rt) return JNI_FALSE;
        const auto path = JStringToPath(env, musicPath);
        const bool ok = rt->music.Init(path, loop == JNI_TRUE);
        return ok ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT jdouble JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMusicLengthSec(
        JNIEnv*, jclass, const jlong handle) {
        const auto* rt = FromHandle(handle);
        if (!rt) return 0.0;
        return rt->music.GetLengthSec();
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeSetMusicVolume(
        JNIEnv*, jclass, const jlong handle, const jfloat volume) {
        auto* rt = FromHandle(handle);
        if (!rt) return;
        rt->music.SetVolume(volume);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeStopMusic(
        JNIEnv*, jclass, const jlong handle) {
        auto* rt = FromHandle(handle);
        if (!rt) return;
        rt->music.Stop();
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetMusicTimes(
        JNIEnv* env, jclass, const jlong handle, _jobject* dst2f) {
        auto* rt = FromHandle(handle);
        if (!rt || !dst2f) return JNI_FALSE;
        auto* out = static_cast<float*>(env->GetDirectBufferAddress(dst2f));
        const jlong cap = env->GetDirectBufferCapacity(dst2f);
        if (!out || cap < static_cast<jlong>(2 * sizeof(float))) return JNI_FALSE;
        if (!rt->music.m_hasSound) {
            out[0] = 0.0f;
            out[1] = 0.0f;
            return JNI_FALSE;
        }
        auto [dt, t] = rt->music.PullTimes();
        out[0] = dt;
        out[1] = t;
        return JNI_TRUE;
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeSyncPhysics(
        JNIEnv*, jclass, const jlong handle, const jfloat t) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->anim) return;
        rt->anim->SyncPhysics(t);
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeUpdate(
        JNIEnv*, jclass, const jlong handle, const jfloat frame, const jfloat physicsElapsed) {
        auto* rt = FromHandle(handle);
        if (!rt || !rt->model) return;
        rt->model->BeginAnimation();
        if (rt->blending && rt->blendToDefault) {
            const float duration = rt->blendDuration;
            const float weight = duration > 0.0f
                ? std::clamp(rt->blendElapsed / duration, 0.0f, 1.0f)
                : 1.0f;
            rt->model->BeginAnimation();
            for (const auto& node : rt->model->m_nodes) {
                node->m_animTranslate = glm::mix(node->m_baseAnimTranslate, glm::vec3(0), weight);
                node->m_animRotate = glm::slerp(node->m_baseAnimRotate, glm::quat(1, 0, 0, 0), weight);
            }
            for (const auto& morph : rt->model->m_morphs)
                morph->m_weight = glm::mix(morph->m_saveAnimWeight, 0.0f, weight);
            for (const auto& ikSolver : rt->model->m_ikSolvers)
                ikSolver->m_enable = weight < 1.0f ? ikSolver->m_baseAnimEnable : true;
            rt->model->ApplyAdditiveRotations();
            rt->model->UpdateMorphAnimation();
            rt->model->UpdateNodeAnimation(false);
            rt->model->UpdatePhysicsAnimation(physicsElapsed);
            rt->model->UpdateNodeAnimation(true);

            rt->blendElapsed += physicsElapsed;
            if (duration <= 0.0f || rt->blendElapsed >= duration) {
                for (const auto& node : rt->model->m_nodes) {
                    node->m_animTranslate = glm::vec3(0);
                    node->m_animRotate = glm::quat(1, 0, 0, 0);
                }
                for (const auto& morph : rt->model->m_morphs)
                    morph->m_weight = 0.0f;
                for (const auto& ikSolver : rt->model->m_ikSolvers)
                    ikSolver->m_enable = true;
                rt->anim = CreateAnimation(rt->model);
                rt->blendAnim.reset();
                rt->blending = false;
                rt->blendToDefault = false;
                rt->blendElapsed = 0.0f;
                rt->blendDuration = 0.0f;
            }
        } else if (rt->blending && rt->blendAnim) {
            const float duration = rt->blendDuration;
            const float weight = duration > 0.0f
                ? std::clamp(rt->blendElapsed / duration, 0.0f, 1.0f)
                : 1.0f;
            rt->blendAnim->Evaluate(frame, weight);
            rt->model->ApplyAdditiveRotations();
            rt->model->UpdateMorphAnimation();
            rt->model->UpdateNodeAnimation(false);
            rt->model->UpdatePhysicsAnimation(physicsElapsed);
            rt->model->UpdateNodeAnimation(true);

            rt->blendElapsed += physicsElapsed;
            if (duration <= 0.0f || rt->blendElapsed >= duration) {
                rt->anim = std::move(rt->blendAnim);
                rt->blending = false;
                rt->blendElapsed = 0.0f;
                rt->blendDuration = 0.0f;
            }
        } else
            rt->model->UpdateAllAnimation(rt->anim.get(), frame, physicsElapsed);
        rt->model->m_parallelUpdateCount = 1;
        rt->model->Update();
    }

    JNIEXPORT void JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeSetBoneRotationAdditive(
        JNIEnv* env, jclass, const jlong handle, _jstring* boneName, const jfloat pitchDeg, const jfloat yawDeg, const jfloat rollDeg) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model || !boneName) return;
        const std::string name = JStringToUtf8(env, boneName);
        if (name.empty()) return;
        const auto it = std::ranges::find(rt->model->m_nodes, name, &Node::m_name);
        if (it == rt->model->m_nodes.end()) return;
        const auto idx = static_cast<size_t>(std::distance(rt->model->m_nodes.begin(), it));
        const float px = glm::radians(pitchDeg);
        const float py = glm::radians(yawDeg);
        const float pz = glm::radians(rollDeg);
        const glm::quat qx = glm::angleAxis(px, glm::vec3(1, 0, 0));
        const glm::quat qy = glm::angleAxis(py, glm::vec3(0, 1, 0));
        const glm::quat qz = glm::angleAxis(pz, glm::vec3(0, 0, 1));
        const glm::quat q = glm::normalize(qy * qx * qz);
        rt->model->m_additiveAnimRotate[idx] = q;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeHasBone(
        JNIEnv* env, jclass, const jlong handle, _jstring* boneName) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model || !boneName) return JNI_FALSE;
        const std::string name = JStringToUtf8(env, boneName);
        if (name.empty()) return JNI_FALSE;
        const auto it = std::ranges::find(rt->model->m_nodes, name, &Node::m_name);
        return it != rt->model->m_nodes.end() ? JNI_TRUE : JNI_FALSE;
    }

    JNIEXPORT jboolean JNICALL Java_net_Chivent_pmxSteveMod_jni_PmxNative_nativeGetBoneGlobalMatrix(
        JNIEnv* env, jclass, const jlong handle, _jstring* boneName, _jobject* dst16f) {
        const auto* rt = FromHandle(handle);
        if (!rt || !rt->model || !boneName || !dst16f) return JNI_FALSE;
        const std::string name = JStringToUtf8(env, boneName);
        if (name.empty()) return JNI_FALSE;
        const auto it = std::ranges::find(rt->model->m_nodes, name, &Node::m_name);
        if (it == rt->model->m_nodes.end()) return JNI_FALSE;
        WriteMat4(env, dst16f, (*it)->m_global);
        return JNI_TRUE;
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
            out[i * 2 + 1] = v.y;
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
        return mat->m_edgeFlag;
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
