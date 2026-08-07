#include "Viewer/Instance/Instance.h"

#include "Core/Animation/Model/Animation.h"
#include "Core/Model/ModelUpdater.h"
#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <utility>

namespace Chrivent {
	Instance::Instance(const GraphicsApi sourceGraphicsApi) : graphicsApi(sourceGraphicsApi) {}
	Instance::~Instance() = default;

	GraphicsError Instance::CreateGraphicsError(const GraphicsErrorCode code, std::string operation,
		std::string message, const int64_t nativeCode, const bool hasNativeCode) const {
		return GraphicsError::Create(graphicsApi, code, std::move(operation),
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

	GraphicsError::Result<void> Instance::CheckInitialized(const char* operation) const {
		if (initialized)
			return {};
		return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
			operation, "모델 인스턴스가 초기화되지 않았습니다"));
	}

	GraphicsError::Result<void> Instance::Initialize(std::shared_ptr<Model> sourceModel,
		std::unique_ptr<Animation> sourceAnimation, const float sourceScale) {
		initialized = false;
		ResetRendererResources();
		model.reset();
		animation.reset();
		scale = 1.0f;
		if (!drawer || !sourceModel || !ValidateModel(*sourceModel)
			|| !std::isfinite(sourceScale)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"모델 인스턴스 초기화",
				"모델 geometry, material 범위 또는 배율이 올바르지 않습니다"));
		}
		model = std::move(sourceModel);
		animation = std::move(sourceAnimation);
		scale = sourceScale;
		const auto setupResult = SetupRenderer();
		if (setupResult) {
			initialized = true;
			return {};
		}
		ResetRendererResources();
		model.reset();
		animation.reset();
		scale = 1.0f;
		return std::unexpected(setupResult.error());
	}

	GraphicsError::Result<void> Instance::Upload() {
		const auto initializedResult = CheckInitialized("모델 버텍스 업로드");
		if (!initializedResult)
			return std::unexpected(initializedResult.error());
		return UploadCore();
	}

	void Instance::BeginDraw(const SceneDrawState& state) const {
		if (!initialized)
			return;
		drawer->BeginDraw(state);
	}

	GraphicsError::Result<void> Instance::DrawModelPass() const {
		const auto initializedResult = CheckInitialized("모델 패스 그리기");
		if (!initializedResult)
			return std::unexpected(initializedResult.error());
		return drawer->DrawModelPass();
	}

	GraphicsError::Result<void> Instance::DrawEdgePass() const {
		const auto initializedResult = CheckInitialized("엣지 패스 그리기");
		if (!initializedResult)
			return std::unexpected(initializedResult.error());
		return drawer->DrawEdgePass();
	}

	GraphicsError::Result<void> Instance::DrawGroundShadowPass() const {
		const auto initializedResult = CheckInitialized("지면 그림자 패스 그리기");
		if (!initializedResult)
			return std::unexpected(initializedResult.error());
		return drawer->DrawGroundShadowPass();
	}

	GraphicsError::Result<void> Instance::DrawPostProcessSceneInputs() const {
		const auto initializedResult = CheckInitialized("후처리 장면 입력 그리기");
		if (!initializedResult)
			return std::unexpected(initializedResult.error());
		return drawer->DrawPostProcessSceneInputs();
	}

	void Instance::PrepareUpdate(const InstanceUpdateState& state, ModelUpdateTiming* timing) const {
		if (!initialized)
			return;
		ModelUpdater::Prepare(*model, {
			.animation = animation.get(),
			.frame = state.animationFrame,
			.physicsElapsed = state.elapsed,
			.preservePreviousPositions = state.velocityRequired,
			.updatePhysics = state.physicsEnabled,
			.timing = timing
		});
	}

	std::size_t Instance::CalculateSkinningTaskCount() const {
		if (!initialized)
			return 0;
		return ModelUpdater::CalculateSkinningTaskCount(*model);
	}

	void Instance::UpdateSkinning(const std::size_t taskIndex) const {
		if (!initialized)
			return;
		ModelUpdater::UpdateSkinning(*model, taskIndex);
	}
}
