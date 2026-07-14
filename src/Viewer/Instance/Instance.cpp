#include "Viewer/Instance/Instance.h"

#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Viewer/Viewer.h"
#include "Core/Model/ModelUpdater.h"

namespace Chrivent {
    Instance::Instance() = default;
    Instance::~Instance() = default;

	bool Instance::Setup(Viewer& baseViewer) {
		Clear();
		if (model == nullptr)
			return false;
		if (SetupRenderer(baseViewer))
			return true;
		Clear();
		return false;
	}

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::DrawPostProcessDepth() const {
        if (drawer)
            drawer->DrawPostProcessDepth();
    }

    void Instance::PrepareUpdate(const Viewer& viewer, const bool physicsEnabled, ModelUpdateTiming* timing) const {
        const ModelUpdater updater(*model);
        updater.Prepare(anim.get(), viewer.animTime * 30.0f, viewer.elapsed,
            viewer.RequiresPostProcessVelocity(), physicsEnabled && !viewer.skipPhysics, timing);
    }

    std::size_t Instance::CalculateSkinningTaskCount() const {
        const ModelUpdater updater(*model);
        return updater.CalculateSkinningTaskCount();
    }

    void Instance::UpdateSkinning(const std::size_t taskIndex) const {
        const ModelUpdater updater(*model);
        updater.UpdateSkinning(taskIndex);
    }
}
