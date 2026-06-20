#include "Instance.h"

#include "Drawer.h"
#include "Viewer.h"
#include "../Animation/Model/Animation.h"
#include "../Model/ModelAnimator.h"
#include "../Model/ModelPose.h"
#include "../Model/ModelSkinning.h"

namespace Chrivent {
    InstanceInfo::InstanceInfo() = default;
    InstanceInfo::~InstanceInfo() = default;

    Instance::Instance() : info(std::make_unique<InstanceInfo>()) {}
    Instance::~Instance() = default;

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::PrepareUpdate(const ViewerInfo& viewerInfo) const {
        const auto& instanceInfo = GetInfo();
        const ModelAnimator animator(*instanceInfo.model);
        animator.BeginAnimation();
        animator.UpdateAllAnimation(instanceInfo.anim.get(), viewerInfo.animTime * 30.0f, viewerInfo.elapsed, !viewerInfo.skipPhysics);
        const ModelPose pose(*instanceInfo.model);
        pose.UpdateTransforms();
        const ModelSkinning skinning(*instanceInfo.model);
        skinning.PrepareUpdate();
    }

    std::size_t Instance::GetSkinningTaskCount() const {
        const ModelSkinning skinning(*GetInfo().model);
        return skinning.GetUpdateRangeCount();
    }

    void Instance::UpdateSkinning(const std::size_t taskIndex) const {
        const ModelSkinning skinning(*GetInfo().model);
        skinning.UpdateRange(taskIndex);
    }
}
