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
		vertexConstants.wvp = viewerInfo.projMat * viewerInfo.viewMat * world;
		if (!info.modelVertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants)))
			std::cerr << "Failed to update Vulkan model vertex constants.\n";
		if (!info.materials.empty()) {
			const auto& material = info.materials.front();
			const auto& mat = material.mat;
			VulkanModelPixelConstants pixelConstants{};
			pixelConstants.alpha = mat.diffuse.a;
			pixelConstants.diffuse = mat.diffuse;
			pixelConstants.ambient = mat.ambient;
			pixelConstants.specularPower = mat.specularPower;
			pixelConstants.specular = mat.specular;
			pixelConstants.lightColor = viewerInfo.lightColor;
			pixelConstants.lightDir = glm::mat3(viewerInfo.viewMat) * viewerInfo.lightDir;
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
			if (!info.modelPixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants)))
				std::cerr << "Failed to update Vulkan model pixel constants.\n";
		}
		info.viewer->BindModelDescriptorSets(info.modelDescriptorSet.GetInfo());
		info.viewer->DrawIndexed(
			info.vertexBuffer.GetInfo(),
			info.indexBuffer.GetInfo(),
			info.indexType,
			info.indexCount);
	}

	void VulkanDrawer::DrawEdge() const {
	}

	void VulkanDrawer::DrawGroundShadow() const {
	}
}
