#include "Viewer/Vulkan/VulkanDrawer.h"

#include "Viewer/Vulkan/VulkanInstance.h"
#include "Viewer/Vulkan/VulkanViewer.h"
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
		if (!instance.viewer->syncObject)
			return;
		const size_t frameIndex = instance.viewer->syncObject->currentFrame;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		instance.modelVertexConstantsRing.BeginFrame(frameIndex);
		instance.modelPixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewer = *instance.viewer;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const glm::vec3 lightDir = glm::mat3(viewer.viewMat) * viewer.lightDir;
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = viewer.viewMat * world;
		vertexConstants.wvp = ClipMatrix() * viewer.projMat * viewer.viewMat * world;
		ModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = glm::vec4(viewer.lightColor, 0.0f);
		basePixelConstants.lightDir = glm::vec4(lightDir, 0.0f);
		std::string error;
		const auto vertexSlice = instance.modelVertexConstantsRing.Allocate(sizeof(vertexConstants), instance.uniformBufferOffsetAlignment, error);
		if (!vertexSlice.has_value() ||
			!instance.modelVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error)) {
			std::cerr << "Failed to update Vulkan model vertex constants.\n";
			return;
		}
		instance.viewer->BindModelDescriptorSets(instance.modelDescriptorSet, vertexSlice->offset);
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants pixelConstants = basePixelConstants;
			pixelConstants.diffuseAlpha = mat.diffuse;
			pixelConstants.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			pixelConstants.specular = glm::vec4(mat.specular, 0.0f);
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			if (material.textureEnabled)
				pixelConstants.textureModes.x = material.texture.hasAlpha ? 2 : 1;
			if (material.toonTextureEnabled)
				pixelConstants.textureModes.y = 1;
			if (material.sphereTextureEnabled) {
				if (mat.spTextureMode == SphereMode::Mul)
					pixelConstants.textureModes.z = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					pixelConstants.textureModes.z = 2;
			}
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
		if (!instance.viewer->syncObject)
			return;
		const size_t frameIndex = instance.viewer->syncObject->currentFrame;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		instance.edgeVertexConstantsRing.BeginFrame(frameIndex);
		instance.edgePixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewer = *instance.viewer;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		EdgeVertexConstants baseVertexConstants{};
		baseVertexConstants.wv = viewer.viewMat * world;
		baseVertexConstants.wvp = ClipMatrix() * viewer.projMat * viewer.viewMat * world;
		baseVertexConstants.screenSize = glm::vec2(viewer.screenWidth, -viewer.screenHeight);
		instance.viewer->BindEdgePipeline();
		std::string error;
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
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
		if (!instance.viewer->syncObject)
			return;
		const size_t frameIndex = instance.viewer->syncObject->currentFrame;
		const auto& vertexBuffer = instance.vertexBuffers[frameIndex % VulkanInstance::kBufferedFrames];
		instance.groundShadowVertexConstantsRing.BeginFrame(frameIndex);
		instance.groundShadowPixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewer = *instance.viewer;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const glm::mat4 shadow = BuildGroundShadowMatrix(viewer.lightDir);
		GroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = ClipMatrix() * viewer.projMat * viewer.viewMat * shadow * world;
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
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

	VulkanDrawer::VulkanDrawer(VulkanInstance& sourceInstance) : instance(sourceInstance) {}
}
