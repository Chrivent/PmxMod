#include "Viewer/Instance/Instance.h"

#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/ModelUpdater.h"

#include <utility>

namespace Chrivent {
    Instance::Instance(const GraphicsApi sourceGraphicsApi) : graphicsApi(sourceGraphicsApi) {}
    Instance::~Instance() = default;

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
			return std::unexpected(GraphicsError{
				.api = graphicsApi,
				.code = GraphicsErrorCode::InvalidArgument,
				.operation = "모델 인스턴스 초기화",
				.message = "모델 geometry 또는 material 범위가 올바르지 않습니다"
			});
		}
        model = std::move(sourceModel);
        animation = std::move(sourceAnimation);
        scale = sourceScale;
        if (SetupRenderer())
            return {};
        ResetRendererResources();
        model.reset();
        animation.reset();
        scale = 1.0f;
        return std::unexpected(GraphicsError{
			.api = graphicsApi,
			.code = GraphicsErrorCode::ResourceCreationFailed,
			.operation = "모델 인스턴스 초기화",
			.message = "모델 GPU 리소스를 만들지 못했습니다"
		});
    }

	GraphicsResult<void> Instance::Upload() {
		if (UploadCore())
			return {};
		return std::unexpected(GraphicsError{
			.api = graphicsApi,
			.code = GraphicsErrorCode::CommandRecordingFailed,
			.operation = "모델 정점 업로드",
			.message = "갱신된 정점 데이터를 GPU 리소스에 반영하지 못했습니다"
		});
	}

	void Instance::BeginDraw(const SceneDrawState& state) const {
		drawer->BeginDraw(state);
	}

	GraphicsResult<void> Instance::DrawModelPass() const {
		if (drawer->DrawModelPass())
			return {};
		return std::unexpected(GraphicsError{
			.api = graphicsApi,
			.code = GraphicsErrorCode::CommandRecordingFailed,
			.operation = "모델 패스 기록",
			.message = "모델 draw 명령을 기록하지 못했습니다"
		});
	}

	GraphicsResult<void> Instance::DrawEdgePass() const {
		if (drawer->DrawEdgePass())
			return {};
		return std::unexpected(GraphicsError{
			.api = graphicsApi,
			.code = GraphicsErrorCode::CommandRecordingFailed,
			.operation = "엣지 패스 기록",
			.message = "엣지 draw 명령을 기록하지 못했습니다"
		});
	}

	GraphicsResult<void> Instance::DrawGroundShadowPass() const {
		if (drawer->DrawGroundShadowPass())
			return {};
		return std::unexpected(GraphicsError{
			.api = graphicsApi,
			.code = GraphicsErrorCode::CommandRecordingFailed,
			.operation = "지면 그림자 패스 기록",
			.message = "지면 그림자 draw 명령을 기록하지 못했습니다"
		});
	}

    GraphicsResult<void> Instance::DrawPostProcessSceneInputs() const {
		if (drawer->DrawPostProcessSceneInputs())
			return {};
		return std::unexpected(GraphicsError{
			.api = graphicsApi,
			.code = GraphicsErrorCode::CommandRecordingFailed,
			.operation = "후처리 장면 입력 패스 기록",
			.message = "depth 또는 velocity draw 명령을 기록하지 못했습니다"
		});
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
