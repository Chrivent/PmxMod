#include "Viewer/Dx12/Dx12Drawer.h"

#include "Viewer/Dx12/Dx12Instance.h"
#include "Viewer/Dx12/Dx12Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"

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
		if (instance.viewer == nullptr || !instance.viewer->IsFrameReady() || instance.model == nullptr || instance.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->commandList.Get();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->frameIndex % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = instance.modelVertexConstantBuffers[frameIndex];
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = viewer.viewMat * world;
		vertexConstants.wvp = ClipMatrix() * viewer.projMat * viewer.viewMat * world;
		if (!vertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
			std::cerr << "Failed to update DX12 model vertex constants.\n";
			return;
		}
		ModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = glm::vec4(viewer.lightColor, 0.0f);
		basePixelConstants.lightDir = glm::vec4(glm::mat3(viewer.viewMat) * viewer.lightDir, 0.0f);
		ID3D12DescriptorHeap* descriptorHeaps[] = { instance.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			if (materialId >= instance.materials.size() || materialId >= instance.modelPixelConstantBuffers.size())
				continue;
			const Dx12Material& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0.0f)
				continue;
			instance.viewer->BindModelPipeline(mat.bothFace);
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
			const Dx12Buffer& pixelConstantBuffer = instance.modelPixelConstantBuffers[materialId][frameIndex];
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
		if (instance.viewer == nullptr || !instance.viewer->IsFrameReady() || instance.model == nullptr || instance.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->commandList.Get();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->frameIndex % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		EdgeVertexConstants vertexConstants{};
		vertexConstants.wv = viewer.viewMat * world;
		vertexConstants.wvp = ClipMatrix() * viewer.projMat * viewer.viewMat * world;
		vertexConstants.screenSize = glm::vec2(viewer.screenWidth, viewer.screenHeight);
		instance.viewer->BindEdgePipeline();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			if (materialId >= instance.materials.size() ||
				materialId >= instance.edgeVertexConstantBuffers.size() ||
				materialId >= instance.edgePixelConstantBuffers.size())
				continue;
			const auto& mat = instance.materials[materialId].mat;
			if (!mat.edgeFlag || mat.diffuse.a == 0.0f)
				continue;
			vertexConstants.edgeSize = mat.edgeSize;
			const Dx12Buffer& vertexConstantBuffer = instance.edgeVertexConstantBuffers[materialId][frameIndex];
			if (!vertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
				std::cerr << "Failed to update DX12 edge vertex constants.\n";
				continue;
			}
			EdgePixelConstants pixelConstants{};
			pixelConstants.edgeColor = mat.edgeColor;
			const Dx12Buffer& edgePixelConstantBuffer = instance.edgePixelConstantBuffers[materialId][frameIndex];
			if (!edgePixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants))) {
				std::cerr << "Failed to update DX12 edge pixel constants.\n";
				continue;
			}
			commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
			commandList->SetGraphicsRootConstantBufferView(1, edgePixelConstantBuffer.ResolveGpuAddress());
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawGroundShadow() {
		if (instance.viewer == nullptr || !instance.viewer->IsFrameReady() || instance.model == nullptr || instance.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->commandList.Get();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->frameIndex % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = instance.groundShadowVertexConstantBuffers[frameIndex];
		const Dx12Buffer& pixelConstantBuffer = instance.groundShadowPixelConstantBuffers[frameIndex];
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const glm::mat4 shadow = BuildGroundShadowMatrix(viewer.lightDir);
		GroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = ClipMatrix() * viewer.projMat * viewer.viewMat * shadow * world;
		if (!vertexConstantBuffer.Write(&vertexConstants, sizeof(vertexConstants))) {
			std::cerr << "Failed to update DX12 ground shadow vertex constants.\n";
			return;
		}
		constexpr GroundShadowPixelConstants pixelConstants{};
		if (!pixelConstantBuffer.Write(&pixelConstants, sizeof(pixelConstants))) {
			std::cerr << "Failed to update DX12 ground shadow pixel constants.\n";
			return;
		}
		instance.viewer->BindGroundShadowPipeline();
		commandList->OMSetStencilRef(0x01);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		commandList->SetGraphicsRootConstantBufferView(1, pixelConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& mat = instance.materials[materialId].mat;
			if (!mat.groundShadow || mat.diffuse.a == 0.0f)
				continue;
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	Dx12Drawer::Dx12Drawer(const Dx12Instance& sourceInstance) : instance(sourceInstance) {}
}
