#include "Instance.h"

#include "Drawer.h"
#include "Viewer.h"
#include "../Core/Model/ModelUpdater.h"

namespace Chrivent {
    Instance::Instance() = default;
    Instance::~Instance() = default;

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::PrepareUpdate(const Viewer& viewer, const bool physicsEnabled, ModelUpdateTiming* timing) const {
        const ModelUpdater updater(*model);
        updater.Prepare(anim.get(), viewer.animTime * 30.0f, viewer.elapsed,
            physicsEnabled && !viewer.skipPhysics, timing);
    }

    std::size_t Instance::GetSkinningTaskCount() const {
        const ModelUpdater updater(*model);
        return updater.GetSkinningTaskCount();
    }

    void Instance::UpdateSkinning(const std::size_t taskIndex) const {
        const ModelUpdater updater(*model);
        updater.UpdateSkinning(taskIndex);
    }
}
