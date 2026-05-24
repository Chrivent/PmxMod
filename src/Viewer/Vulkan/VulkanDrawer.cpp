#include "VulkanDrawer.h"

#include "VulkanInstance.h"
#include "VulkanViewer.h"
#include "../ShaderConstants.h"
#include "../../Model/Model.h"

#include <iostream>

namespace Chrivent {
	VulkanDrawer::VulkanDrawer(const VulkanInstanceInfo& sourceInfo) : info(sourceInfo) {}

	void VulkanDrawer::DrawModel() const {
		if (info.viewer == nullptr)
			return;
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = viewerInfo.viewMat * world;
		vertexConstants.wvp = VulkanClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * world;
		if (!info.modelVertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants)))
			std::cerr << "Failed to update Vulkan model vertex constants.\n";
		info.viewer->BindModelDescriptorSets(info.modelDescriptorSet.GetInfo());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants pixelConstants{};
			pixelConstants.diffuseAlpha = glm::vec4(mat.diffuse.r, mat.diffuse.g, mat.diffuse.b, mat.diffuse.a);
			pixelConstants.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			pixelConstants.specular = glm::vec4(mat.specular, 0.0f);
			pixelConstants.lightColor = glm::vec4(viewerInfo.lightColor, 0.0f);
			pixelConstants.lightDir = glm::vec4(glm::mat3(viewerInfo.viewMat) * viewerInfo.lightDir, 0.0f);
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			if (!mat.texture.empty())
				pixelConstants.textureModes.x = material.texture.hasAlpha ? 2 : 1;
			if (!mat.toonTexture.empty())
				pixelConstants.textureModes.y = 1;
			if (!mat.spTexture.empty()) {
				if (mat.spTextureMode == SphereMode::Mul)
					pixelConstants.textureModes.z = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					pixelConstants.textureModes.z = 2;
			}
			if (!material.pixelConstantBuffer ||
				!material.pixelConstantBuffer->Write(&pixelConstants, sizeof(pixelConstants)))
				std::cerr << "Failed to update Vulkan model pixel constants.\n";
			info.viewer->BindModelPipeline(mat.bothFace);
			info.viewer->BindPixelDescriptorSet(material.pixelDescriptorSet);
			info.viewer->BindTextureDescriptorSet(material.textureDescriptorSet);
			info.viewer->DrawIndexed(
				info.vertexBuffer.GetInfo(),
				info.indexBuffer.GetInfo(),
				info.indexType,
				beginIndex,
				indexCount);
		}
	}

	void VulkanDrawer::DrawEdge() const {
		if (info.viewer == nullptr)
			return;
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		info.viewer->BindEdgePipeline();
		info.viewer->BindModelDescriptorSets(info.edgeDescriptorSet.GetInfo());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag || mat.diffuse.a == 0.0f)
				continue;
			EdgeVertexConstants vertexConstants;
			vertexConstants.wv = viewerInfo.viewMat * world;
			vertexConstants.wvp = VulkanClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * world;
			vertexConstants.screenSize = glm::vec2(viewerInfo.screenWidth, -viewerInfo.screenHeight);
			vertexConstants.edgeSize = mat.edgeSize;
			if (!info.edgeVertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants)))
				std::cerr << "Failed to update Vulkan edge vertex constants.\n";
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			if (!material.edgePixelConstantBuffer ||
				!material.edgePixelConstantBuffer->Write(&pixelConstants, sizeof(pixelConstants)))
				std::cerr << "Failed to update Vulkan edge pixel constants.\n";
			info.viewer->BindPixelDescriptorSet(material.edgePixelDescriptorSet);
			info.viewer->DrawIndexed(
				info.vertexBuffer.GetInfo(),
				info.indexBuffer.GetInfo(),
				info.indexType,
				beginIndex,
				indexCount);
		}
	}

	void VulkanDrawer::DrawGroundShadow() const {
		if (info.viewer == nullptr)
			return;
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		constexpr glm::vec4 plane(0.0f, 1.0f, 0.0f, 0.0f);
		const glm::vec4 light(-viewerInfo.lightDir, 0.0f);
		const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
		GroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = VulkanClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * shadow * world;
		if (!info.groundShadowVertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants)))
			std::cerr << "Failed to update Vulkan ground shadow vertex constants.\n";
		info.viewer->BindGroundShadowPipeline();
		info.viewer->BindModelDescriptorSets(info.groundShadowDescriptorSet.GetInfo());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow || mat.diffuse.a == 0.0f)
				continue;
			GroundShadowPixelConstants pixelConstants;
			if (!material.groundShadowPixelConstantBuffer ||
				!material.groundShadowPixelConstantBuffer->Write(&pixelConstants, sizeof(pixelConstants)))
				std::cerr << "Failed to update Vulkan ground shadow pixel constants.\n";
			info.viewer->BindPixelDescriptorSet(material.groundShadowPixelDescriptorSet);
			info.viewer->DrawIndexed(
				info.vertexBuffer.GetInfo(),
				info.indexBuffer.GetInfo(),
				info.indexType,
				beginIndex,
				indexCount);
		}
	}

	const glm::mat4& VulkanDrawer::VulkanClipMatrix() {
		static constexpr glm::mat4 vulkanMat(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return vulkanMat;
	}
}
