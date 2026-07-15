#include "Viewer/Drawer/VulkanDrawer.h"

#include "Viewer/Instance/VulkanInstance.h"
#include "Viewer/Viewer/VulkanViewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"

#include <iostream>

namespace Chrivent {
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
		if (instance.viewer == nullptr)
			return;
		if (!instance.viewer->modelEffectEnabled)
			return;
		size_t frameIndex = 0;
		if (!instance.viewer->ResolveFrameIndex(frameIndex))
			return;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		instance.modelVertexConstantsRing.BeginFrame(frameIndex);
		instance.modelPixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewer = *instance.viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		std::string error;
		const auto vertexSlice = instance.modelVertexConstantsRing.Allocate(sizeof(vertexConstants), instance.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!instance.modelVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error)) {
			std::cerr << "Failed to update Vulkan model vertex constants.\n";
			return;
		}
		instance.viewer->BindModelDescriptorSets(instance.modelDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			const int textureMode = material.textureEnabled ? material.texture.hasAlpha ? 2 : 1 : 0;
			const int toonTextureMode = material.toonTextureEnabled ? 1 : 0;
			int sphereTextureMode = 0;
			if (material.sphereTextureEnabled) {
				if (mat.spTextureMode == SphereMode::Mul)
					sphereTextureMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					sphereTextureMode = 2;
			}
			const ModelPixelConstants pixelConstants = BuildModelPixelConstants(
				viewer, mat, textureMode, toonTextureMode, sphereTextureMode);
			const auto pixelSlice = instance.modelPixelConstantsRing.Allocate(sizeof(pixelConstants), instance.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value() ||
				!instance.modelPixelConstantsRing.Write(*pixelSlice, &pixelConstants, error)) {
				std::cerr << "Failed to update Vulkan model pixel constants.\n";
				continue;
			}
			instance.viewer->BindModelPipeline(mat.bothFace);
			instance.viewer->BindPixelDescriptorSet(material.pixelDescriptorSet, pixelSlice->offset);
			instance.viewer->BindTextureDescriptorSet(material.textureDescriptorSet);
			instance.viewer->DrawIndexed(vertexBuffer, instance.indexBuffer, instance.indexType, beginIndex, indexCount);
		}
	}

	void VulkanDrawer::DrawEdge() {
		if (instance.viewer == nullptr)
			return;
		if (!instance.viewer->edgeEffectEnabled)
			return;
		size_t frameIndex = 0;
		if (!instance.viewer->ResolveFrameIndex(frameIndex))
			return;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		instance.edgeVertexConstantsRing.BeginFrame(frameIndex);
		instance.edgePixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewer = *instance.viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.screenWidth, -viewer.screenHeight));
		instance.viewer->BindEdgePipeline();
		std::string error;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag || mat.diffuse.a == 0.0f)
				continue;
			EdgeVertexConstants vertexConstants = baseVertexConstants;
			vertexConstants.edgeSize = mat.edgeSize;
			const auto vertexSlice = instance.edgeVertexConstantsRing.Allocate(sizeof(vertexConstants), instance.uniformBufferOffsetAlignment, error);
			if (!vertexSlice.has_value() ||
				!instance.edgeVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error)) {
				std::cerr << "Failed to update Vulkan edge vertex constants.\n";
				continue;
			}
			instance.viewer->BindModelDescriptorSets(instance.edgeDescriptorSet, vertexSlice->offset);
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			const auto pixelSlice = instance.edgePixelConstantsRing.Allocate(sizeof(pixelConstants), instance.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value() ||
				!instance.edgePixelConstantsRing.Write(*pixelSlice, &pixelConstants, error)) {
				std::cerr << "Failed to update Vulkan edge pixel constants.\n";
				continue;
			}
			instance.viewer->BindPixelDescriptorSet(material.edgePixelDescriptorSet, pixelSlice->offset);
			instance.viewer->DrawIndexed(vertexBuffer, instance.indexBuffer, instance.indexType, beginIndex, indexCount);
		}
	}

	void VulkanDrawer::DrawGroundShadow() {
		if (instance.viewer == nullptr)
			return;
		if (!instance.viewer->groundShadowEffectEnabled)
			return;
		size_t frameIndex = 0;
		if (!instance.viewer->ResolveFrameIndex(frameIndex))
			return;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		instance.groundShadowVertexConstantsRing.BeginFrame(frameIndex);
		instance.groundShadowPixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewer = *instance.viewer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
		constexpr GroundShadowPixelConstants pixelConstants{};
		std::string error;
		const auto vertexSlice = instance.groundShadowVertexConstantsRing.Allocate(sizeof(vertexConstants), instance.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!instance.groundShadowVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error)) {
			std::cerr << "Failed to update Vulkan ground shadow vertex constants.\n";
			return;
		}
		const auto pixelSlice = instance.groundShadowPixelConstantsRing.Allocate(sizeof(pixelConstants), instance.uniformBufferOffsetAlignment, error);
		if (!pixelSlice.has_value() ||
			!instance.groundShadowPixelConstantsRing.Write(*pixelSlice, &pixelConstants, error)) {
			std::cerr << "Failed to update Vulkan ground shadow pixel constants.\n";
			return;
		}
		instance.viewer->BindGroundShadowPipeline();
		instance.viewer->BindModelDescriptorSets(instance.groundShadowDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow || mat.diffuse.a == 0.0f)
				continue;
			instance.viewer->BindPixelDescriptorSet(material.groundShadowPixelDescriptorSet, pixelSlice->offset);
			instance.viewer->DrawIndexed(vertexBuffer, instance.indexBuffer, instance.indexType, beginIndex, indexCount);
		}
	}

	void VulkanDrawer::DrawSceneInputs() {
		if (instance.viewer == nullptr)
			return;
		size_t frameIndex = 0;
		if (!instance.viewer->ResolveFrameIndex(frameIndex))
			return;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		const auto& viewer = *instance.viewer;
		if (!viewer.modelEffectEnabled) {
			instance.modelVertexConstantsRing.BeginFrame(frameIndex);
			instance.modelPixelConstantsRing.BeginFrame(frameIndex);
		}
		const auto world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			viewer, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		const bool velocityRequired = viewer.RequiresPostProcessVelocity();
		std::string error;
		const size_t constantSize = velocityRequired ? sizeof(velocityConstants) : sizeof(depthConstants);
		const auto vertexSlice = instance.modelVertexConstantsRing.Allocate(
			constantSize, instance.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!instance.modelVertexConstantsRing.Write(*vertexSlice,
				velocityRequired ? static_cast<const void*>(&velocityConstants) : &depthConstants, error)) {
			std::cerr << "Failed to update Vulkan scene input vertex constants.\n";
			return;
		}
		instance.viewer->BindModelDescriptorSets(instance.modelDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.textureEnabled && material.texture.hasAlpha);
			const auto pixelSlice = instance.modelPixelConstantsRing.Allocate(
				sizeof(ModelPixelConstants), instance.uniformBufferOffsetAlignment, error);
			if (!pixelSlice.has_value()
				|| !instance.modelPixelConstantsRing.Write(*pixelSlice, &pixelConstants, error)) {
				std::cerr << "Failed to update Vulkan scene surface constants.\n";
				continue;
			}
			if (velocityRequired)
				instance.viewer->BindSceneVelocityPipeline(mat.bothFace);
			else
				instance.viewer->BindDepthOnlyPipeline(mat.bothFace);
			instance.viewer->BindPixelDescriptorSet(material.pixelDescriptorSet, pixelSlice->offset);
			instance.viewer->BindTextureDescriptorSet(material.textureDescriptorSet);
			instance.viewer->DrawIndexed(vertexBuffer, instance.indexBuffer, instance.indexType, beginIndex, indexCount);
		}
	}

	VulkanDrawer::VulkanDrawer(VulkanInstance& sourceInstance) : instance(sourceInstance) {}
}
