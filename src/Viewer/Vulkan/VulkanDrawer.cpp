#include "VulkanDrawer.h"

#include "VulkanInstance.h"
#include "VulkanViewer.h"
#include "../Assist/ViewerMatrix.h"
#include "../Assist/ViewerTextureMode.h"
#include "../Assist/Glsl/GlslShaderConstants.h"
#include "../../Model/Model.h"

#include <iostream>

namespace Chrivent {
	void VulkanDrawer::DrawModel() {
		if (info.viewer == nullptr)
			return;
		const auto& vulkanInfo = info.viewer->GetVulkanInfo();
		if (vulkanInfo.syncInfo == nullptr)
			return;
		const size_t frameIndex = vulkanInfo.syncInfo->currentFrame;
		const auto& vertexBuffer = info.vertexBuffers[frameIndex % VulkanInstanceInfo::kBufferedFrames];
		info.modelVertexConstantsRing.BeginFrame(frameIndex);
		info.modelPixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const glm::vec3 lightDir = glm::mat3(viewerInfo.viewMat) * viewerInfo.lightDir;
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = viewerInfo.viewMat * world;
		vertexConstants.wvp = ViewerMatrix::VulkanClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * world;
		ModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = glm::vec4(viewerInfo.lightColor, 0.0f);
		basePixelConstants.lightDir = glm::vec4(lightDir, 0.0f);
		std::string error;
		const auto vertexSlice = info.modelVertexConstantsRing.Allocate(
			sizeof(vertexConstants),
			info.uniformBufferOffsetAlignment,
			error);
		if (!vertexSlice.has_value() ||
			!info.modelVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error))
			std::cerr << "Failed to update Vulkan model vertex constants.\n";
		info.viewer->BindModelDescriptorSets(info.modelDescriptorSet, vertexSlice.has_value() ? vertexSlice->offset : 0);
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants pixelConstants = basePixelConstants;
			pixelConstants.diffuseAlpha = glm::vec4(mat.diffuse.r, mat.diffuse.g, mat.diffuse.b, mat.diffuse.a);
			pixelConstants.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			pixelConstants.specular = glm::vec4(mat.specular, 0.0f);
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			pixelConstants.textureModes.x = ViewerTextureMode::Base(!mat.texture.empty(), material.texture.hasAlpha);
			pixelConstants.textureModes.y = ViewerTextureMode::Toon(!mat.toonTexture.empty());
			pixelConstants.textureModes.z = ViewerTextureMode::Sphere(!mat.spTexture.empty(), mat.spTextureMode);
			const auto pixelSlice = info.modelPixelConstantsRing.Allocate(
				sizeof(pixelConstants),
				info.uniformBufferOffsetAlignment,
				error);
			if (!pixelSlice.has_value() ||
				!info.modelPixelConstantsRing.Write(*pixelSlice, &pixelConstants, error))
				std::cerr << "Failed to update Vulkan model pixel constants.\n";
			info.viewer->BindModelPipeline(mat.bothFace);
			info.viewer->BindPixelDescriptorSet(material.pixelDescriptorSet, pixelSlice.has_value() ? pixelSlice->offset : 0);
			info.viewer->BindTextureDescriptorSet(material.textureDescriptorSet);
			info.viewer->DrawIndexed(
				vertexBuffer.GetInfo(),
				info.indexBuffer.GetInfo(),
				info.indexType,
				beginIndex,
				indexCount);
		}
	}

	void VulkanDrawer::DrawEdge() {
		if (info.viewer == nullptr)
			return;
		const auto& vulkanInfo = info.viewer->GetVulkanInfo();
		if (vulkanInfo.syncInfo == nullptr)
			return;
		const size_t frameIndex = vulkanInfo.syncInfo->currentFrame;
		const auto& vertexBuffer = info.vertexBuffers[frameIndex % VulkanInstanceInfo::kBufferedFrames];
		info.edgeVertexConstantsRing.BeginFrame(frameIndex);
		info.edgePixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		EdgeVertexConstants baseVertexConstants{};
		baseVertexConstants.wv = viewerInfo.viewMat * world;
		baseVertexConstants.wvp = ViewerMatrix::VulkanClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * world;
		baseVertexConstants.screenSize = glm::vec2(viewerInfo.screenWidth, -viewerInfo.screenHeight);
		info.viewer->BindEdgePipeline();
		std::string error;
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag || mat.diffuse.a == 0.0f)
				continue;
			EdgeVertexConstants vertexConstants = baseVertexConstants;
			vertexConstants.edgeSize = mat.edgeSize;
			const auto vertexSlice = info.edgeVertexConstantsRing.Allocate(
				sizeof(vertexConstants),
				info.uniformBufferOffsetAlignment,
				error);
			if (!vertexSlice.has_value() ||
				!info.edgeVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error))
				std::cerr << "Failed to update Vulkan edge vertex constants.\n";
			info.viewer->BindModelDescriptorSets(info.edgeDescriptorSet, vertexSlice.has_value() ? vertexSlice->offset : 0);
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			const auto pixelSlice = info.edgePixelConstantsRing.Allocate(
				sizeof(pixelConstants),
				info.uniformBufferOffsetAlignment,
				error);
			if (!pixelSlice.has_value() ||
				!info.edgePixelConstantsRing.Write(*pixelSlice, &pixelConstants, error))
				std::cerr << "Failed to update Vulkan edge pixel constants.\n";
			info.viewer->BindPixelDescriptorSet(material.edgePixelDescriptorSet, pixelSlice.has_value() ? pixelSlice->offset : 0);
			info.viewer->DrawIndexed(
				vertexBuffer.GetInfo(),
				info.indexBuffer.GetInfo(),
				info.indexType,
				beginIndex,
				indexCount);
		}
	}

	void VulkanDrawer::DrawGroundShadow() {
		if (info.viewer == nullptr)
			return;
		const auto& vulkanInfo = info.viewer->GetVulkanInfo();
		if (vulkanInfo.syncInfo == nullptr)
			return;
		const size_t frameIndex = vulkanInfo.syncInfo->currentFrame;
		const auto& vertexBuffer = info.vertexBuffers[frameIndex % VulkanInstanceInfo::kBufferedFrames];
		info.groundShadowVertexConstantsRing.BeginFrame(frameIndex);
		info.groundShadowPixelConstantsRing.BeginFrame(frameIndex);
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const glm::mat4 shadow = ViewerMatrix::BuildGroundShadowMatrix(viewerInfo.lightDir);
		GroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = ViewerMatrix::VulkanClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * shadow * world;
		constexpr GroundShadowPixelConstants pixelConstants{};
		std::string error;
		const auto vertexSlice = info.groundShadowVertexConstantsRing.Allocate(
			sizeof(vertexConstants),
			info.uniformBufferOffsetAlignment,
			error);
		if (!vertexSlice.has_value() ||
			!info.groundShadowVertexConstantsRing.Write(*vertexSlice, &vertexConstants, error))
			std::cerr << "Failed to update Vulkan ground shadow vertex constants.\n";
		const auto pixelSlice = info.groundShadowPixelConstantsRing.Allocate(
			sizeof(pixelConstants),
			info.uniformBufferOffsetAlignment,
			error);
		if (!pixelSlice.has_value() ||
			!info.groundShadowPixelConstantsRing.Write(*pixelSlice, &pixelConstants, error))
			std::cerr << "Failed to update Vulkan ground shadow pixel constants.\n";
		info.viewer->BindGroundShadowPipeline();
		info.viewer->BindModelDescriptorSets(info.groundShadowDescriptorSet, vertexSlice.has_value() ? vertexSlice->offset : 0);
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow || mat.diffuse.a == 0.0f)
				continue;
			info.viewer->BindPixelDescriptorSet(material.groundShadowPixelDescriptorSet, pixelSlice.has_value() ? pixelSlice->offset : 0);
			info.viewer->DrawIndexed(
				vertexBuffer.GetInfo(),
				info.indexBuffer.GetInfo(),
				info.indexType,
				beginIndex,
				indexCount);
		}
	}

	VulkanDrawer::VulkanDrawer(VulkanInstanceInfo& sourceInfo) : info(sourceInfo) {}
}
