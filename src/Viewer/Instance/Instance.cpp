#include "Viewer/Instance/Instance.h"

#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/ModelUpdater.h"

#include <utility>

namespace Chrivent {
    Instance::Instance(const GraphicsApi sourceGraphicsApi) : graphicsApi(sourceGraphicsApi) {}
    Instance::~Instance() = default;

	GraphicsError Instance::CreateGraphicsError(const GraphicsErrorCode code, std::string operation,
		std::string message, const int64_t nativeCode, const bool hasNativeCode) const {
		return MakeGraphicsError(graphicsApi, code, std::move(operation),
			std::move(message), nativeCode, hasNativeCode);
	}

	bool Instance::ValidateModel(const Model& sourceModel) {
		if (sourceModel.geometryData.positions.empty() || !ViewerGeometry::ValidateIndexData(sourceModel.geometryData))
			return false;
		const size_t indexCount = sourceModel.geometryData.indexCount;
		const size_t materialCount = sourceModel.materialData.materials.size();
		for (const auto& [beginIndex, subMeshIndexCount, materialId] : sourceModel.materialData.subMeshes) {
			if (materialId >= materialCount || beginIndex > indexCount || subMeshIndexCount > indexCount - beginIndex)
				return false;
		}
		return true;
	}

    GraphicsResult<void> Instance::Initialize(std::shared_ptr<Model> sourceModel,
        std::unique_ptr<Animation> sourceAnimation, const float sourceScale) {
        ResetRendererResources();
        model.reset();
        animation.reset();
        scale = 1.0f;
        if (!drawer || !sourceModel || !ValidateModel(*sourceModel)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"모델 인스턴스 초기화", "모델 geometry 또는 material 범위가 올바르지 않습니다"));
		}
        model = std::move(sourceModel);
        animation = std::move(sourceAnimation);
        scale = sourceScale;
        const auto setupResult = SetupRenderer();
		if (setupResult)
            return {};
        ResetRendererResources();
        model.reset();
        animation.reset();
        scale = 1.0f;
        return std::unexpected(setupResult.error());
    }

	GraphicsResult<void> Instance::Upload() {
		return UploadCore();
	}

	void Instance::BeginDraw(const SceneDrawState& state) const {
		drawer->BeginDraw(state);
	}

	GraphicsResult<void> Instance::DrawModelPass() const {
		return drawer->DrawModelPass();
	}

	GraphicsResult<void> Instance::DrawEdgePass() const {
		return drawer->DrawEdgePass();
	}

	GraphicsResult<void> Instance::DrawGroundShadowPass() const {
		return drawer->DrawGroundShadowPass();
	}

    GraphicsResult<void> Instance::DrawPostProcessSceneInputs() const {
		return drawer->DrawPostProcessSceneInputs();
    }

	void Instance::PrepareUpdate(const InstanceUpdateState& state, ModelUpdateTiming* timing) const {
        const ModelUpdater updater(*model);
		updater.Prepare(animation.get(), state.animationFrame, state.elapsed,
			state.velocityRequired, state.physicsEnabled, timing);
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
