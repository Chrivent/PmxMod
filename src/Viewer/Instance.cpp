#include "Instance.h"

#include "Drawer.h"
#include "Viewer.h"
#include "../Animation/Model/Animation.h"
#include "../Model/ModelAnimator.h"

namespace Chrivent {
    Instance::Instance() = default;
    Instance::~Instance() = default;

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::UpdateAnimation(const Viewer& viewer) const {
        const ModelAnimator animator(*info.model);
        animator.BeginAnimation();
        animator.UpdateAllAnimation(info.anim.get(), viewer.animTime * 30.0f, viewer.elapsed, !viewer.skipPhysics);
    }
}
