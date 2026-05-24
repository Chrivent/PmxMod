#include "VulkanDrawer.h"

#include "VulkanInstance.h"
#include "VulkanViewer.h"
#include "Helper/VulkanConstants.h"
#include "../../Model/Model.h"

#include <iostream>

namespace Chrivent {
	VulkanDrawer::VulkanDrawer(const VulkanInstanceInfo& sourceInfo) : info(sourceInfo) {}

	void VulkanDrawer::DrawModel() const {
		if (info.viewer == nullptr)
			return;
		const auto& viewerInfo = info.viewer->GetInfo();
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		VulkanModelVertexConstants vertexConstants;
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
			VulkanModelPixelConstants pixelConstants{};
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
	}

	void VulkanDrawer::DrawGroundShadow() const {
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
