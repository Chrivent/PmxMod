#include "Instance.h"

#include "Drawer.h"
#include "Viewer.h"
#include "../Model/ModelUpdater.h"

namespace Chrivent {
    InstanceInfo::InstanceInfo() = default;
    InstanceInfo::~InstanceInfo() = default;

    Instance::Instance() : info(std::make_unique<InstanceInfo>()) {}
    Instance::~Instance() = default;

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::PrepareUpdate(const ViewerInfo& viewerInfo, ModelUpdateTiming* timing) const {
        const auto& instanceInfo = GetInfo();
        const ModelUpdater updater(*instanceInfo.model);
        updater.Prepare(
            instanceInfo.anim.get(),
            viewerInfo.animTime * 30.0f,
            viewerInfo.elapsed,
            !viewerInfo.skipPhysics,
            timing);
    }

    std::size_t Instance::GetSkinningTaskCount() const {
        const ModelUpdater updater(*GetInfo().model);
        return updater.GetSkinningTaskCount();
    }

    void Instance::UpdateSkinning(const std::size_t taskIndex) const {
        const ModelUpdater updater(*GetInfo().model);
        updater.UpdateSkinning(taskIndex);
    }
}
