#include "Viewer/Instance/Instance.h"

#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Viewer/Viewer.h"
#include "Core/Model/ModelUpdater.h"

#include <utility>

namespace Chrivent {
    Instance::Instance() = default;
    Instance::~Instance() = default;

	bool Instance::ValidateModel(const Model& sourceModel) {
		const size_t indexCount = sourceModel.geometryData.indexCount;
		const size_t materialCount = sourceModel.materialData.materials.size();
		for (const auto& [beginIndex, subMeshIndexCount, materialId] : sourceModel.materialData.subMeshes) {
			if (materialId >= materialCount || beginIndex > indexCount || subMeshIndexCount > indexCount - beginIndex)
				return false;
		}
		return true;
	}

    bool Instance::Initialize(std::shared_ptr<Model> sourceModel,
        std::unique_ptr<Animation> sourceAnimation, const float sourceScale) {
        ResetRendererResources();
        model.reset();
        animation.reset();
        scale = 1.0f;
        if (!sourceModel || !ValidateModel(*sourceModel))
            return false;
        model = std::move(sourceModel);
        animation = std::move(sourceAnimation);
        scale = sourceScale;
        if (SetupRenderer())
            return true;
        ResetRendererResources();
        model.reset();
        animation.reset();
        scale = 1.0f;
        return false;
    }

    void Instance::Draw() const {
        if (drawer)
            drawer->Draw();
    }

    void Instance::DrawPostProcessSceneInputs() const {
        if (drawer)
            drawer->DrawPostProcessSceneInputs();
    }

    void Instance::PrepareUpdate(const Viewer& viewer, const bool physicsEnabled, ModelUpdateTiming* timing) const {
        const ModelUpdater updater(*model);
        updater.Prepare(animation.get(), viewer.animTime * 30.0f, viewer.elapsed,
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
