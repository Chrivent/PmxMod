#include "Instance.h"

#include "Drawer.h"
#include "Viewer.h"
#include "../Animation/Model/Animation.h"
#include "../Model/ModelAnimator.h"

namespace Chrivent {
    InstanceInfo::InstanceInfo() = default;
    InstanceInfo::~InstanceInfo() = default;

    Instance::Instance() : info(std::make_unique<InstanceInfo>()) {}
    Instance::~Instance() = default;

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::UpdateAnimation(const Viewer& viewer) const {
        const auto& instanceInfo = GetInfo();
        const ModelAnimator animator(*instanceInfo.model);
        animator.BeginAnimation();
        animator.UpdateAllAnimation(instanceInfo.anim.get(), viewer.animTime * 30.0f, viewer.elapsed, !viewer.skipPhysics);
    }
}
