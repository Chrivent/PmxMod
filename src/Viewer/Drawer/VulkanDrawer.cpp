#include "Viewer/Drawer/VulkanDrawer.h"

#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Instance/VulkanInstance.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"

#include <iostream>

namespace Chrivent {
	void VulkanDrawer::BeginDrawFrame() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		resources.modelVertexConstantsRing.BeginFrame(frameIndex);
		resources.edgeVertexConstantsRing.BeginFrame(frameIndex);
		resources.groundShadowVertexConstantsRing.BeginFrame(frameIndex);
		resources.modelPixelConstantsRing.BeginFrame(frameIndex);
		resources.edgePixelConstantsRing.BeginFrame(frameIndex);
		resources.groundShadowPixelConstantsRing.BeginFrame(frameIndex);
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

	bool VulkanDrawer::DrawModel() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		std::string error;
		const auto vertexSlice = resources.modelVertexConstantsRing.Allocate(
			sizeof(vertexConstants), resources.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!resources.modelVertexConstantsRing.Write(*vertexSlice, &vertexConstants, sizeof(vertexConstants), error)) {
			std::cerr << "Vulkan 모델 vertex 상수를 갱신하지 못했습니다.\n";
			return false;
		}
		drawContext.BindModelDescriptorSets(resources.modelDescriptorSet, vertexSlice->offset);
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
			const auto pixelSlice = resources.modelPixelConstantsRing.Allocate(
				sizeof(pixelConstants), resources.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value() ||
				!resources.modelPixelConstantsRing.Write(*pixelSlice, &pixelConstants, sizeof(pixelConstants), error)) {
				std::cerr << "Vulkan 모델 pixel 상수를 갱신하지 못했습니다.\n";
				return false;
			}
			drawContext.BindModelPipeline(mat.bothFace);
			drawContext.BindPixelDescriptorSet(
				resources.modelDescriptorSet.GetPixelDescriptorSet(), pixelSlice->offset);
			drawContext.BindTextureDescriptorSet(material.textureDescriptorSet);
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount)) {
				std::cerr << "Vulkan 모델 indexed draw 명령을 기록하지 못했습니다.\n";
				return false;
			}
		}
		return true;
	}

	bool VulkanDrawer::DrawEdge() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			drawState, world, ClipMatrix(), glm::vec2(drawState.screenSize.x, -drawState.screenSize.y));
		drawContext.BindEdgePipeline();
		std::string error;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			EdgeVertexConstants vertexConstants = baseVertexConstants;
			vertexConstants.edgeSize = mat.edgeSize;
			const auto vertexSlice = resources.edgeVertexConstantsRing.Allocate(
				sizeof(vertexConstants), resources.uniformBufferOffsetAlignment, error);
			if (!vertexSlice.has_value() ||
				!resources.edgeVertexConstantsRing.Write(*vertexSlice, &vertexConstants, sizeof(vertexConstants), error)) {
				std::cerr << "Vulkan 엣지 vertex 상수를 갱신하지 못했습니다.\n";
				return false;
			}
			drawContext.BindModelDescriptorSets(resources.edgeDescriptorSet, vertexSlice->offset);
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			const auto pixelSlice = resources.edgePixelConstantsRing.Allocate(
				sizeof(pixelConstants), resources.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value() ||
				!resources.edgePixelConstantsRing.Write(*pixelSlice, &pixelConstants, sizeof(pixelConstants), error)) {
				std::cerr << "Vulkan 엣지 pixel 상수를 갱신하지 못했습니다.\n";
				return false;
			}
			drawContext.BindPixelDescriptorSet(
				resources.edgeDescriptorSet.GetPixelDescriptorSet(), pixelSlice->offset);
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount)) {
				std::cerr << "Vulkan 엣지 indexed draw 명령을 기록하지 못했습니다.\n";
				return false;
			}
		}
		return true;
	}

	bool VulkanDrawer::DrawGroundShadow() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			drawState, world, ClipMatrix());
		constexpr GroundShadowPixelConstants pixelConstants{};
		std::string error;
		const auto vertexSlice = resources.groundShadowVertexConstantsRing.Allocate(
			sizeof(vertexConstants), resources.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!resources.groundShadowVertexConstantsRing.Write(*vertexSlice, &vertexConstants,
				sizeof(vertexConstants), error)) {
			std::cerr << "Vulkan 지면 그림자 vertex 상수를 갱신하지 못했습니다.\n";
			return false;
		}
		const auto pixelSlice = resources.groundShadowPixelConstantsRing.Allocate(
			sizeof(pixelConstants), resources.uniformBufferOffsetAlignment, error);
		if (!pixelSlice.has_value() ||
			!resources.groundShadowPixelConstantsRing.Write(*pixelSlice, &pixelConstants,
				sizeof(pixelConstants), error)) {
			std::cerr << "Vulkan 지면 그림자 pixel 상수를 갱신하지 못했습니다.\n";
			return false;
		}
		drawContext.BindGroundShadowPipeline();
		drawContext.BindModelDescriptorSets(resources.groundShadowDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			drawContext.BindPixelDescriptorSet(
				resources.groundShadowDescriptorSet.GetPixelDescriptorSet(), pixelSlice->offset);
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount)) {
				std::cerr << "Vulkan 지면 그림자 indexed draw 명령을 기록하지 못했습니다.\n";
				return false;
			}
		}
		return true;
	}

	bool VulkanDrawer::DrawSceneInputs() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			drawState, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		const bool velocityRequired = drawState.velocityRequired;
		std::string error;
		const size_t constantSize = velocityRequired ? sizeof(velocityConstants) : sizeof(depthConstants);
		const auto vertexSlice = resources.modelVertexConstantsRing.Allocate(
			constantSize, resources.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!resources.modelVertexConstantsRing.Write(*vertexSlice,
				velocityRequired ? static_cast<const void*>(&velocityConstants) : &depthConstants, constantSize, error)) {
			std::cerr << "Vulkan 장면 입력 vertex 상수를 갱신하지 못했습니다.\n";
			return false;
		}
		drawContext.BindModelDescriptorSets(resources.modelDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.textureEnabled && material.texture.hasAlpha);
			const auto pixelSlice = resources.modelPixelConstantsRing.Allocate(
				sizeof(ModelPixelConstants), resources.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value()
				|| !resources.modelPixelConstantsRing.Write(*pixelSlice, &pixelConstants,
					sizeof(pixelConstants), error)) {
				std::cerr << "Vulkan 장면 표면 상수를 갱신하지 못했습니다.\n";
				return false;
			}
			if (velocityRequired)
				drawContext.BindSceneVelocityPipeline(mat.bothFace);
			else
				drawContext.BindDepthOnlyPipeline(mat.bothFace);
			drawContext.BindPixelDescriptorSet(
				resources.modelDescriptorSet.GetPixelDescriptorSet(), pixelSlice->offset);
			drawContext.BindTextureDescriptorSet(material.textureDescriptorSet);
			if (!drawContext.DrawIndexed(
				vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount)) {
				std::cerr << "Vulkan 후처리 장면 입력 indexed draw 명령을 기록하지 못했습니다.\n";
				return false;
			}
		}
		return true;
	}

	VulkanDrawer::VulkanDrawer(const VulkanInstance& sourceInstance, VulkanModelResources& sourceResources,
		VulkanDrawContext& sourceDrawContext)
		: instance(sourceInstance), resources(sourceResources), drawContext(sourceDrawContext) {}
}
