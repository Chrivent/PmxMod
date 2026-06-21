#include "Dx12Drawer.h"

#include "Dx12Instance.h"
#include "Dx12Viewer.h"
#include "../HlslShaderConstants.h"
#include "../../../Core/Model/Model.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace Chrivent {
	const glm::mat4& Dx12Drawer::ClipMatrix() const {
		static constexpr glm::mat4 clipMatrix(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return clipMatrix;
	}

	void Dx12Drawer::DrawModel() {
		if (info.viewer == nullptr || info.model == nullptr || info.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = info.viewer->GetDx12Info().commandList.Get();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = info.viewer->GetDx12Info().frameIndex % Dx12InstanceInfo::kBufferedFrames;
		const auto& vertexBufferView = info.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = info.modelVertexConstantBuffers[frameIndex];
		const auto& viewerInfo = info.viewer->GetInfo();
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		HlslModelVertexConstants vertexConstants;
		vertexConstants.wv = viewerInfo.viewMat * world;
		vertexConstants.wvp = ClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * world;
		if (!vertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
			std::cerr << "Failed to update DX12 model vertex constants.\n";
			return;
		}
		HlslModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = viewerInfo.lightColor;
		basePixelConstants.lightDir = glm::mat3(viewerInfo.viewMat) * viewerInfo.lightDir;
		ID3D12DescriptorHeap* descriptorHeaps[] = { info.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&info.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size() || materialId >= info.modelPixelConstantBuffers.size())
				continue;
			const Dx12Material& material = info.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0.0f)
				continue;
			info.viewer->BindModelPipelineState(mat.bothFace);
			HlslModelPixelConstants pixelConstants = basePixelConstants;
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
			const Dx12Buffer& pixelConstantBuffer = info.modelPixelConstantBuffers[materialId][frameIndex];
			if (!pixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants))) {
				std::cerr << "Failed to update DX12 model pixel constants.\n";
				continue;
			}
			commandList->SetGraphicsRootConstantBufferView(1, pixelConstantBuffer.ResolveGpuAddress());
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawEdge() {
		if (info.viewer == nullptr || info.model == nullptr || info.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = info.viewer->GetDx12Info().commandList.Get();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = info.viewer->GetDx12Info().frameIndex % Dx12InstanceInfo::kBufferedFrames;
		const auto& vertexBufferView = info.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = info.edgeVertexConstantBuffers[frameIndex];
		const auto& viewerInfo = info.viewer->GetInfo();
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		HlslEdgeVertexConstants vertexConstants{};
		vertexConstants.wv = viewerInfo.viewMat * world;
		vertexConstants.wvp = ClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * world;
		vertexConstants.screenSize = glm::vec2(viewerInfo.screenWidth, viewerInfo.screenHeight);
		if (!vertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
			std::cerr << "Failed to update DX12 edge vertex constants.\n";
			return;
		}
		info.viewer->BindEdgePipelineState();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&info.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size() ||
				materialId >= info.edgeSizeConstantBuffers.size() ||
				materialId >= info.edgePixelConstantBuffers.size())
				continue;
			const auto& mat = info.materials[materialId].mat;
			if (!mat.edgeFlag || mat.diffuse.a == 0.0f)
				continue;
			HlslEdgeSizeConstants edgeSizeConstants{};
			edgeSizeConstants.edgeSize = mat.edgeSize;
			const Dx12Buffer& edgeSizeConstantBuffer = info.edgeSizeConstantBuffers[materialId][frameIndex];
			if (!edgeSizeConstantBuffer.Write(&edgeSizeConstants, sizeof(edgeSizeConstants))) {
				std::cerr << "Failed to update DX12 edge size constants.\n";
				continue;
			}
			HlslEdgePixelConstants pixelConstants{};
			pixelConstants.edgeColor = mat.edgeColor;
			const Dx12Buffer& edgePixelConstantBuffer = info.edgePixelConstantBuffers[materialId][frameIndex];
			if (!edgePixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants))) {
				std::cerr << "Failed to update DX12 edge pixel constants.\n";
				continue;
			}
			commandList->SetGraphicsRootConstantBufferView(1, edgeSizeConstantBuffer.ResolveGpuAddress());
			commandList->SetGraphicsRootConstantBufferView(2, edgePixelConstantBuffer.ResolveGpuAddress());
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawGroundShadow() {
		if (info.viewer == nullptr || info.model == nullptr || info.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = info.viewer->GetDx12Info().commandList.Get();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = info.viewer->GetDx12Info().frameIndex % Dx12InstanceInfo::kBufferedFrames;
		const auto& vertexBufferView = info.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = info.groundShadowVertexConstantBuffers[frameIndex];
		const Dx12Buffer& pixelConstantBuffer = info.groundShadowPixelConstantBuffers[frameIndex];
		const auto& viewerInfo = info.viewer->GetInfo();
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const glm::mat4 shadow = BuildGroundShadowMatrix(viewerInfo.lightDir);
		HlslGroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = ClipMatrix() * viewerInfo.projMat * viewerInfo.viewMat * shadow * world;
		if (!vertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
			std::cerr << "Failed to update DX12 ground shadow vertex constants.\n";
			return;
		}
		constexpr HlslGroundShadowPixelConstants pixelConstants{};
		if (!pixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants))) {
			std::cerr << "Failed to update DX12 ground shadow pixel constants.\n";
			return;
		}
		info.viewer->BindGroundShadowPipelineState();
		commandList->OMSetStencilRef(0x01);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&info.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		commandList->SetGraphicsRootConstantBufferView(1, pixelConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			if (materialId >= info.materials.size())
				continue;
			const auto& mat = info.materials[materialId].mat;
			if (!mat.groundShadow || mat.diffuse.a == 0.0f)
				continue;
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	Dx12Drawer::Dx12Drawer(const Dx12InstanceInfo& sourceInfo) : info(sourceInfo) {}
}
