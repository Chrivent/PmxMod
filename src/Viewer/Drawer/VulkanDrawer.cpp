#include "Viewer/Drawer/VulkanDrawer.h"

#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Instance/VulkanInstance.h"
#include "Viewer/Viewer/Viewer.h"
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

	void VulkanDrawer::DrawModel() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto& viewer = this->viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		std::string error;
		const auto vertexSlice = resources.modelVertexConstantsRing.Allocate(
			sizeof(vertexConstants), resources.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!resources.modelVertexConstantsRing.Write(*vertexSlice, &vertexConstants, sizeof(vertexConstants), error)) {
			std::cerr << "Failed to update Vulkan model vertex constants.\n";
			return;
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
				viewer, mat, base, toon, sphere);
			const auto pixelSlice = resources.modelPixelConstantsRing.Allocate(
				sizeof(pixelConstants), resources.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value() ||
				!resources.modelPixelConstantsRing.Write(*pixelSlice, &pixelConstants, sizeof(pixelConstants), error)) {
				std::cerr << "Failed to update Vulkan model pixel constants.\n";
				continue;
			}
			drawContext.BindModelPipeline(mat.bothFace);
			drawContext.BindPixelDescriptorSet(material.pixelDescriptorSet, pixelSlice->offset);
			drawContext.BindTextureDescriptorSet(material.textureDescriptorSet);
			drawContext.DrawIndexed(vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount);
		}
	}

	void VulkanDrawer::DrawEdge() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto& viewer = this->viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.GetScreenWidth(), -viewer.GetScreenHeight()));
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
				std::cerr << "Failed to update Vulkan edge vertex constants.\n";
				continue;
			}
			drawContext.BindModelDescriptorSets(resources.edgeDescriptorSet, vertexSlice->offset);
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			const auto pixelSlice = resources.edgePixelConstantsRing.Allocate(
				sizeof(pixelConstants), resources.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value() ||
				!resources.edgePixelConstantsRing.Write(*pixelSlice, &pixelConstants, sizeof(pixelConstants), error)) {
				std::cerr << "Failed to update Vulkan edge pixel constants.\n";
				continue;
			}
			drawContext.BindPixelDescriptorSet(material.edgePixelDescriptorSet, pixelSlice->offset);
			drawContext.DrawIndexed(vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount);
		}
	}

	void VulkanDrawer::DrawGroundShadow() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto& viewer = this->viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
		constexpr GroundShadowPixelConstants pixelConstants{};
		std::string error;
		const auto vertexSlice = resources.groundShadowVertexConstantsRing.Allocate(
			sizeof(vertexConstants), resources.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!resources.groundShadowVertexConstantsRing.Write(*vertexSlice, &vertexConstants,
				sizeof(vertexConstants), error)) {
			std::cerr << "Failed to update Vulkan ground shadow vertex constants.\n";
			return;
		}
		const auto pixelSlice = resources.groundShadowPixelConstantsRing.Allocate(
			sizeof(pixelConstants), resources.uniformBufferOffsetAlignment, error);
		if (!pixelSlice.has_value() ||
			!resources.groundShadowPixelConstantsRing.Write(*pixelSlice, &pixelConstants,
				sizeof(pixelConstants), error)) {
			std::cerr << "Failed to update Vulkan ground shadow pixel constants.\n";
			return;
		}
		drawContext.BindGroundShadowPipeline();
		drawContext.BindModelDescriptorSets(resources.groundShadowDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			drawContext.BindPixelDescriptorSet(material.groundShadowPixelDescriptorSet, pixelSlice->offset);
			drawContext.DrawIndexed(vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount);
		}
	}

	void VulkanDrawer::DrawSceneInputs() {
		const size_t frameIndex = drawContext.GetFrameIndex();
		const auto& vertexBuffer = resources.vertexBuffers[frameIndex % FrameBuffering::vulkanFramesInFlight];
		const auto& viewer = this->viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			viewer, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		const bool velocityRequired = viewer.RequiresPostProcessVelocity();
		std::string error;
		const size_t constantSize = velocityRequired ? sizeof(velocityConstants) : sizeof(depthConstants);
		const auto vertexSlice = resources.modelVertexConstantsRing.Allocate(
			constantSize, resources.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!resources.modelVertexConstantsRing.Write(*vertexSlice,
				velocityRequired ? static_cast<const void*>(&velocityConstants) : &depthConstants, constantSize, error)) {
			std::cerr << "Failed to update Vulkan scene input vertex constants.\n";
			return;
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
				std::cerr << "Failed to update Vulkan scene surface constants.\n";
				continue;
			}
			if (velocityRequired)
				drawContext.BindSceneVelocityPipeline(mat.bothFace);
			else
				drawContext.BindDepthOnlyPipeline(mat.bothFace);
			drawContext.BindPixelDescriptorSet(material.pixelDescriptorSet, pixelSlice->offset);
			drawContext.BindTextureDescriptorSet(material.textureDescriptorSet);
			drawContext.DrawIndexed(vertexBuffer, resources.indexBuffer, resources.indexType, beginIndex, indexCount);
		}
	}

	VulkanDrawer::VulkanDrawer(const VulkanInstance& sourceInstance, VulkanModelResources& sourceResources,
		VulkanDrawContext& sourceDrawContext, Viewer& sourceViewer)
		: Drawer(sourceViewer), instance(sourceInstance), resources(sourceResources),
		drawContext(sourceDrawContext) {}
}
