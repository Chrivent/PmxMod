#include "jni_bridge.h"

#include "src/MMDModel.h"
#include "src/VMDCameraAnimation.h"

#include <memory>

struct Core {
    std::unique_ptr<saba::MMDModel> model;
    std::unique_ptr<saba::VMDCameraAnimation> cameraAnimation;
};

static Core* from(jlong h) { return reinterpret_cast<Core*>(h); }

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_vmdCameraAnim_reset(JNIEnv *, jclass, jlong h) {
    if (auto* c = from(h)) { c->cameraAnimation.reset(); }
}





JNIEXPORT jlong JNICALL Java_net_chrivent_pmxstevemod_src_Native_create(JNIEnv*, jclass) {
    auto* core = new Core();
    return reinterpret_cast<jlong>(core);
}

JNIEXPORT void JNICALL Java_net_chrivent_pmxstevemod_src_Native_destroy(JNIEnv*, jclass, jlong h) {
    if (auto* c = from(h)) {
        c->model.reset();
        c->cameraAnimation.reset();
        delete c;
    }
}
