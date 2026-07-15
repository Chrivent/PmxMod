#include "Viewer/Drawer/Dx12Drawer.h"

#include "Viewer/Instance/Dx12Instance.h"
#include "Viewer/Viewer/Dx12Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"

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
		if (instance.viewer == nullptr || instance.indexCount == 0)
			return;
		if (!instance.viewer->modelEffectEnabled)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->GetFrameIndex() % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = instance.modelVertexConstantBuffers[frameIndex];
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		if (!vertexConstantBuffer.Write(vertexConstants)) {
			std::cerr << "Failed to update DX12 model vertex constants.\n";
			return;
		}
		ID3D12DescriptorHeap* descriptorHeaps[] = { instance.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size() || materialId >= instance.modelPixelConstantBuffers.size())
				continue;
			const Dx12Material& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0.0f)
				continue;
			instance.viewer->BindModelPipeline(mat.bothFace);
			const int textureMode = material.texture.resource ? material.texture.hasAlpha ? 2 : 1 : 0;
			const int toonTextureMode = material.toonTexture.resource ? 1 : 0;
			int sphereTextureMode = 0;
			if (material.sphereTexture.resource) {
				if (mat.spTextureMode == SphereMode::Mul)
					sphereTextureMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					sphereTextureMode = 2;
			}
			const ModelPixelConstants pixelConstants = BuildModelPixelConstants(
				viewer, mat, textureMode, toonTextureMode, sphereTextureMode);
			const Dx12Buffer& pixelConstantBuffer = instance.modelPixelConstantBuffers[materialId][frameIndex];
			if (!pixelConstantBuffer.Write(pixelConstants)) {
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
		if (instance.viewer == nullptr || instance.indexCount == 0)
			return;
		if (!instance.viewer->edgeEffectEnabled)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->GetFrameIndex() % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		EdgeVertexConstants vertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.screenWidth, viewer.screenHeight));
		instance.viewer->BindEdgePipeline();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size() ||
				materialId >= instance.edgeVertexConstantBuffers.size() ||
				materialId >= instance.edgePixelConstantBuffers.size())
				continue;
			const auto& mat = instance.materials[materialId].mat;
			if (!mat.edgeFlag || mat.diffuse.a == 0.0f)
				continue;
			vertexConstants.edgeSize = mat.edgeSize;
			const Dx12Buffer& vertexConstantBuffer = instance.edgeVertexConstantBuffers[materialId][frameIndex];
			if (!vertexConstantBuffer.Write(vertexConstants)) {
				std::cerr << "Failed to update DX12 edge vertex constants.\n";
				continue;
			}
			EdgePixelConstants pixelConstants{};
			pixelConstants.edgeColor = mat.edgeColor;
			const Dx12Buffer& edgePixelConstantBuffer = instance.edgePixelConstantBuffers[materialId][frameIndex];
			if (!edgePixelConstantBuffer.Write(pixelConstants)) {
				std::cerr << "Failed to update DX12 edge pixel constants.\n";
				continue;
			}
			commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
			commandList->SetGraphicsRootConstantBufferView(1, edgePixelConstantBuffer.ResolveGpuAddress());
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawGroundShadow() {
		if (instance.viewer == nullptr || instance.indexCount == 0)
			return;
		if (!instance.viewer->groundShadowEffectEnabled)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->GetFrameIndex() % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = instance.groundShadowVertexConstantBuffers[frameIndex];
		const Dx12Buffer& pixelConstantBuffer = instance.groundShadowPixelConstantBuffers[frameIndex];
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
		if (!vertexConstantBuffer.Write(vertexConstants)) {
			std::cerr << "Failed to update DX12 ground shadow vertex constants.\n";
			return;
		}
		constexpr GroundShadowPixelConstants pixelConstants{};
		if (!pixelConstantBuffer.Write(pixelConstants)) {
			std::cerr << "Failed to update DX12 ground shadow pixel constants.\n";
			return;
		}
		instance.viewer->BindGroundShadowPipeline();
		commandList->OMSetStencilRef(0x01);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		commandList->SetGraphicsRootConstantBufferView(1, pixelConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size())
				continue;
			const auto& mat = instance.materials[materialId].mat;
			if (!mat.groundShadow || mat.diffuse.a == 0.0f)
				continue;
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawSceneInputs() {
		if (instance.viewer == nullptr || instance.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = instance.viewer->ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = instance.viewer->GetFrameIndex() % Dx12Instance::kBufferedFrames;
		const auto& vertexBufferView = instance.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = instance.modelVertexConstantBuffers[frameIndex];
		const size_t constantOffset = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelVertexConstants));
		const size_t sceneSurfaceConstantOffset = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelPixelConstants));
		const auto& viewer = *instance.viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			viewer, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		const bool velocityRequired = viewer.RequiresPostProcessVelocity();
		if (!(velocityRequired ? vertexConstantBuffer.Write(velocityConstants, constantOffset)
			: vertexConstantBuffer.Write(depthConstants, constantOffset))) {
			std::cerr << "Failed to update DX12 scene input vertex constants.\n";
			return;
		}
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&instance.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(
			0, vertexConstantBuffer.ResolveGpuAddress() + constantOffset);
		ID3D12DescriptorHeap* descriptorHeaps[] = { instance.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			if (materialId >= instance.materials.size() || materialId >= instance.modelPixelConstantBuffers.size())
				continue;
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.resource && material.texture.hasAlpha);
			const Dx12Buffer& pixelConstantBuffer = instance.modelPixelConstantBuffers[materialId][frameIndex];
			if (!pixelConstantBuffer.Write(pixelConstants, sceneSurfaceConstantOffset)) {
				std::cerr << "Failed to update DX12 scene surface constants.\n";
				continue;
			}
			if (velocityRequired)
				instance.viewer->BindSceneVelocityPipeline(mat.bothFace);
			else
				instance.viewer->BindDepthOnlyPipeline(mat.bothFace);
			commandList->SetGraphicsRootConstantBufferView(
				1, pixelConstantBuffer.ResolveGpuAddress() + sceneSurfaceConstantOffset);
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	Dx12Drawer::Dx12Drawer(const Dx12Instance& sourceInstance) : instance(sourceInstance) {}
}
