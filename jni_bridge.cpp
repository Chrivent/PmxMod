#include "jni_bridge.h"

#include "src/MMDModel.h"
#include "src/VMDCameraAnimation.h"
#include "src/VMDFile.h"

#include <memory>

struct MMDModelCore {
    std::unique_ptr<saba::MMDModel> pmxModel;
};

struct VMDCameraAnimationCore {
    std::unique_ptr<saba::VMDCameraAnimation> vmdCamAnim;
};

struct VMDFileCore {
    saba::VMDFile vmdFile;
};

static MMDModelCore* MMDModelFrom(jlong h) { return reinterpret_cast<MMDModelCore*>(h); }
static VMDCameraAnimationCore* VMDCameraAnimationFrom(jlong h) { return reinterpret_cast<VMDCameraAnimationCore*>(h); }
static VMDFileCore* VMDFileFrom(jlong h) { return reinterpret_cast<VMDFileCore*>(h); }

// 객체 생성 및 해제

JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_1new(JNIEnv*, jclass) {
    auto* core = new MMDModelCore();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_MMDModelCore_1delete(JNIEnv*, jclass, jlong h) {
    if (auto* c = MMDModelFrom(h)) {
        c->pmxModel.reset();
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

JNIEXPORT jboolean JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1Create(JNIEnv *, jclass, jlong camH, jlong vmdH) {
    if (auto* cam = VMDCameraAnimationFrom(camH))
    {
        if (auto* vf = VMDFileFrom(vmdH))
            return cam->vmdCamAnim->Create(vf->vmdFile);
    }
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCamAnim_1reset(JNIEnv *, jclass, jlong h) {
    if (auto* VMDCameraAnimation = VMDCameraAnimationFrom(h))
        VMDCameraAnimation->vmdCamAnim.reset();
}
