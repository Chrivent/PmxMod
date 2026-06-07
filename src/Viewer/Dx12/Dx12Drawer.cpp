#include "Dx12Drawer.h"

#include "Dx12Instance.h"
#include "Dx12Viewer.h"
#include "Helper/Dx12Constants.h"
#include "../../Model/Model.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace Chrivent {
	void Dx12Drawer::DrawModel() {
		if (info.viewer == nullptr || info.model == nullptr || info.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = info.viewer->GetCommandList();
		if (commandList == nullptr)
			return;
		const auto& viewerInfo = info.viewer->GetInfo();
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		constexpr glm::mat4 dxClipMatrix(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		Dx12ModelVertexConstants vertexConstants;
		vertexConstants.wv = viewerInfo.viewMat * world;
		vertexConstants.wvp = dxClipMatrix * viewerInfo.projMat * viewerInfo.viewMat * world;
		if (!info.modelVertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
			std::cerr << "Failed to update DX12 model vertex constants.\n";
			return;
		}
		Dx12ModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = viewerInfo.lightColor;
		basePixelConstants.lightDir = glm::mat3(viewerInfo.viewMat) * viewerInfo.lightDir;
		ID3D12DescriptorHeap* descriptorHeaps[] = { info.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &info.vertexBufferView);
		commandList->IASetIndexBuffer(&info.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, info.modelVertexConstantBuffer.GetGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size() || materialId >= info.modelPixelConstantBuffers.size())
				continue;
			const Dx12Material& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0.0f)
				continue;
			Dx12ModelPixelConstants pixelConstants = basePixelConstants;
			pixelConstants.alpha = mat.diffuse.a;
			pixelConstants.diffuse = mat.diffuse;
			pixelConstants.ambient = mat.ambient;
			pixelConstants.specular = mat.specular;
			pixelConstants.specularPower = mat.specularPower;
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			if (material.texture.resource)
				pixelConstants.textureModes.x = material.texture.hasAlpha ? 2 : 1;
			if (material.toonTexture.resource)
				pixelConstants.textureModes.y = 1;
			if (material.sphereTexture.resource) {
				if (mat.spTextureMode == SphereMode::Mul)
					pixelConstants.textureModes.z = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					pixelConstants.textureModes.z = 2;
			}
			const Dx12Buffer& pixelConstantBuffer = info.modelPixelConstantBuffers[materialId];
			if (!pixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants))) {
				std::cerr << "Failed to update DX12 model pixel constants.\n";
				continue;
			}
			commandList->SetGraphicsRootConstantBufferView(1, pixelConstantBuffer.GetGpuAddress());
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawEdge() {}

	void Dx12Drawer::DrawGroundShadow() {}

	Dx12Drawer::Dx12Drawer(const Dx12InstanceInfo& sourceInfo) : info(sourceInfo) {}
}
