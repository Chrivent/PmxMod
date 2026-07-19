#include "Viewer/Drawer/VulkanDrawer.h"

#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Instance/VulkanInstance.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"

namespace Chrivent {
	DynamicBufferError::Result<UploadSlice> VulkanDrawer::UploadConstants(
		VulkanDynamicBufferRing& ring, const size_t alignment, const void* data, const size_t size) {
		const auto sliceResult = ring.Allocate(size, alignment);
		if (!sliceResult)
			return std::unexpected(sliceResult.error());
		const auto writeResult = ring.Write(*sliceResult, data, size);
		if (!writeResult)
			return std::unexpected(writeResult.error());
		return *sliceResult;
	}

	void VulkanDrawer::BeginDrawFrame() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		resources.vertexConstantsRing.BeginFrame(frameIndex);
		resources.pixelConstantsRing.BeginFrame(frameIndex);
	}

	const glm::mat4& VulkanDrawer::ClipMatrix() const {
		static constexpr glm::mat4 clipMatrix(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return clipMatrix;
	}

	GraphicsError::Result<void> VulkanDrawer::DrawModel() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		const auto vertexSliceResult = UploadConstants(resources.vertexConstantsRing,
			resources.uniformBufferOffsetAlignment, &vertexConstants, sizeof(vertexConstants));
		if (!vertexSliceResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 모델 패스 기록", vertexSliceResult.error().message));
		}
		const UploadSlice& vertexSlice = *vertexSliceResult;
		if (!drawContext.BindModelDescriptorSets(
			resources.modelDescriptorSet, vertexSlice.offset)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 모델 패스 기록", "모델 vertex descriptor set을 바인딩하지 못했습니다"));
		}
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawModelMaterial(mat))
				continue;
			const auto [base, toon, sphere] = ResolveMaterialTextureModes(mat,
				material.textureEnabled, material.texture.hasAlpha,
				material.toonTextureEnabled, material.sphereTextureEnabled);
			const ModelPixelConstants pixelConstants = BuildModelPixelConstants(
				drawState, mat, base, toon, sphere);
			const auto pixelSliceResult = UploadConstants(resources.pixelConstantsRing,
				resources.uniformBufferOffsetAlignment, &pixelConstants, sizeof(pixelConstants));
			if (!pixelSliceResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 모델 패스 기록", pixelSliceResult.error().message));
			}
			const UploadSlice& pixelSlice = *pixelSliceResult;
			if (!drawContext.BindModelPipeline(mat.bothFace)
				|| !drawContext.BindPixelDescriptorSet(
					resources.modelDescriptorSet.GetPixelDescriptorSet(), pixelSlice.offset)
				|| !drawContext.BindTextureDescriptorSet(material.textureDescriptorSet)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 모델 패스 기록", "모델 pipeline 또는 descriptor set을 바인딩하지 못했습니다"));
			}
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 모델 패스 기록", "indexed draw 명령을 기록하지 못했습니다"));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanDrawer::DrawEdge() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			drawState, world, ClipMatrix(), glm::vec2(drawState.screenSize.x, -drawState.screenSize.y));
		if (!drawContext.BindEdgePipeline()) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 엣지 패스 기록", "엣지 pipeline을 바인딩하지 못했습니다"));
		}
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			EdgeVertexConstants vertexConstants = baseVertexConstants;
			vertexConstants.edgeSize = mat.edgeSize;
			const auto vertexSliceResult = UploadConstants(resources.vertexConstantsRing,
				resources.uniformBufferOffsetAlignment, &vertexConstants, sizeof(vertexConstants));
			if (!vertexSliceResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 엣지 패스 기록", vertexSliceResult.error().message));
			}
			const UploadSlice& vertexSlice = *vertexSliceResult;
			if (!drawContext.BindModelDescriptorSets(
				resources.edgeDescriptorSet, vertexSlice.offset)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 엣지 패스 기록", "엣지 vertex descriptor set을 바인딩하지 못했습니다"));
			}
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			const auto pixelSliceResult = UploadConstants(resources.pixelConstantsRing,
				resources.uniformBufferOffsetAlignment, &pixelConstants, sizeof(pixelConstants));
			if (!pixelSliceResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 엣지 패스 기록", pixelSliceResult.error().message));
			}
			const UploadSlice& pixelSlice = *pixelSliceResult;
			if (!drawContext.BindPixelDescriptorSet(
				resources.edgeDescriptorSet.GetPixelDescriptorSet(), pixelSlice.offset)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 엣지 패스 기록", "엣지 pixel descriptor set을 바인딩하지 못했습니다"));
			}
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 엣지 패스 기록", "indexed draw 명령을 기록하지 못했습니다"));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanDrawer::DrawGroundShadow() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			drawState, world, ClipMatrix());
		constexpr GroundShadowPixelConstants pixelConstants{};
		const auto vertexSliceResult = UploadConstants(resources.vertexConstantsRing,
			resources.uniformBufferOffsetAlignment, &vertexConstants, sizeof(vertexConstants));
		if (!vertexSliceResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 지면 그림자 패스 기록", vertexSliceResult.error().message));
		}
		const auto pixelSliceResult = UploadConstants(resources.pixelConstantsRing,
			resources.uniformBufferOffsetAlignment, &pixelConstants, sizeof(pixelConstants));
		if (!pixelSliceResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 지면 그림자 패스 기록", pixelSliceResult.error().message));
		}
		const UploadSlice& vertexSlice = *vertexSliceResult;
		const UploadSlice& pixelSlice = *pixelSliceResult;
		if (!drawContext.BindGroundShadowPipeline()
			|| !drawContext.BindModelDescriptorSets(
				resources.groundShadowDescriptorSet, vertexSlice.offset)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 지면 그림자 패스 기록",
				"지면 그림자 pipeline 또는 vertex descriptor set을 바인딩하지 못했습니다"));
		}
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			if (!drawContext.BindPixelDescriptorSet(
				resources.groundShadowDescriptorSet.GetPixelDescriptorSet(), pixelSlice.offset)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 지면 그림자 패스 기록",
					"지면 그림자 pixel descriptor set을 바인딩하지 못했습니다"));
			}
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 지면 그림자 패스 기록", "indexed draw 명령을 기록하지 못했습니다"));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanDrawer::DrawSceneInputs() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			drawState, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		const bool velocityRequired = drawState.velocityRequired;
		const size_t constantSize = velocityRequired ? sizeof(velocityConstants) : sizeof(depthConstants);
		const void* vertexData = velocityRequired
			? static_cast<const void*>(&velocityConstants) : &depthConstants;
		const auto vertexSliceResult = UploadConstants(resources.vertexConstantsRing,
			resources.uniformBufferOffsetAlignment, vertexData, constantSize);
		if (!vertexSliceResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 후처리 장면 입력 기록", vertexSliceResult.error().message));
		}
		const UploadSlice& vertexSlice = *vertexSliceResult;
		if (!drawContext.BindModelDescriptorSets(
			resources.modelDescriptorSet, vertexSlice.offset)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"Vulkan 후처리 장면 입력 기록",
				"장면 입력 vertex descriptor set을 바인딩하지 못했습니다"));
		}
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.textureEnabled && material.texture.hasAlpha);
			const auto pixelSliceResult = UploadConstants(resources.pixelConstantsRing,
				resources.uniformBufferOffsetAlignment, &pixelConstants, sizeof(pixelConstants));
			if (!pixelSliceResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 후처리 장면 입력 기록", pixelSliceResult.error().message));
			}
			const UploadSlice& pixelSlice = *pixelSliceResult;
			const bool pipelineBound = velocityRequired
				? drawContext.BindSceneVelocityPipeline(mat.bothFace)
				: drawContext.BindSceneDepthPipeline(mat.bothFace);
			if (!pipelineBound || !drawContext.BindPixelDescriptorSet(
				resources.modelDescriptorSet.GetPixelDescriptorSet(), pixelSlice.offset)
				|| !drawContext.BindTextureDescriptorSet(material.textureDescriptorSet)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 후처리 장면 입력 기록",
					"장면 입력 pipeline 또는 descriptor set을 바인딩하지 못했습니다"));
			}
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"Vulkan 후처리 장면 입력 기록", "indexed draw 명령을 기록하지 못했습니다"));
		}
		return {};
	}

	VulkanDrawer::VulkanDrawer(const VulkanInstance& sourceInstance, VulkanModelResources& sourceResources,
		VulkanDrawContext& sourceDrawContext)
		: Drawer(GraphicsApi::Vulkan), instance(sourceInstance),
		resources(sourceResources), drawContext(sourceDrawContext) {}
}
