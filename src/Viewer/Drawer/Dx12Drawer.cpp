#include "Viewer/Drawer/Dx12Drawer.h"

#include "Viewer/DrawContext/Dx12DrawContext.h"
#include "Viewer/Instance/Dx12Instance.h"
#include "Viewer/Viewer/Viewer.h"
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
		if (resources.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = drawContext.ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = resources.modelVertexConstantBuffers[frameIndex];
		const auto& viewer = this->viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		if (!vertexConstantBuffer.Write(vertexConstants)) {
			std::cerr << "Failed to update DX12 model vertex constants.\n";
			return;
		}
		ID3D12DescriptorHeap* descriptorHeaps[] = { resources.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const Dx12ModelMaterial& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawModelMaterial(mat))
				continue;
			drawContext.BindModelPipeline(mat.bothFace);
			const auto [base, toon, sphere] = ResolveMaterialTextureModes(mat,
				material.texture.resource != nullptr, material.texture.hasAlpha,
				material.toonTexture.resource != nullptr, material.sphereTexture.resource != nullptr);
			const ModelPixelConstants pixelConstants = BuildModelPixelConstants(
				viewer, mat, base, toon, sphere);
			const Dx12Buffer& pixelConstantBuffer = resources.modelPixelConstantBuffers[materialId][frameIndex];
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
		if (resources.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = drawContext.ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const auto& viewer = this->viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		EdgeVertexConstants vertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.screenWidth, viewer.screenHeight));
		drawContext.BindEdgePipeline();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& mat = resources.materials[materialId].material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			vertexConstants.edgeSize = mat.edgeSize;
			const Dx12Buffer& vertexConstantBuffer = resources.edgeVertexConstantBuffers[materialId][frameIndex];
			if (!vertexConstantBuffer.Write(vertexConstants)) {
				std::cerr << "Failed to update DX12 edge vertex constants.\n";
				continue;
			}
			EdgePixelConstants pixelConstants{};
			pixelConstants.edgeColor = mat.edgeColor;
			const Dx12Buffer& edgePixelConstantBuffer = resources.edgePixelConstantBuffers[materialId][frameIndex];
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
		if (resources.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = drawContext.ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = resources.groundShadowVertexConstantBuffers[frameIndex];
		const Dx12Buffer& pixelConstantBuffer = resources.groundShadowPixelConstantBuffers[frameIndex];
		const auto& viewer = this->viewer;
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
		drawContext.BindGroundShadowPipeline();
		commandList->OMSetStencilRef(0x01);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, vertexConstantBuffer.ResolveGpuAddress());
		commandList->SetGraphicsRootConstantBufferView(1, pixelConstantBuffer.ResolveGpuAddress());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& mat = resources.materials[materialId].material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	void Dx12Drawer::DrawSceneInputs() {
		if (resources.indexCount == 0)
			return;
		ID3D12GraphicsCommandList* commandList = drawContext.ResolveCommandList();
		if (commandList == nullptr)
			return;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& vertexConstantBuffer = resources.modelVertexConstantBuffers[frameIndex];
		const size_t constantOffset = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelVertexConstants));
		const size_t sceneSurfaceConstantOffset = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelPixelConstants));
		const auto& viewer = this->viewer;
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
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(
			0, vertexConstantBuffer.ResolveGpuAddress() + constantOffset);
		ID3D12DescriptorHeap* descriptorHeaps[] = { resources.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.resource && material.texture.hasAlpha);
			const Dx12Buffer& pixelConstantBuffer = resources.modelPixelConstantBuffers[materialId][frameIndex];
			if (!pixelConstantBuffer.Write(pixelConstants, sceneSurfaceConstantOffset)) {
				std::cerr << "Failed to update DX12 scene surface constants.\n";
				continue;
			}
			if (velocityRequired)
				drawContext.BindSceneVelocityPipeline(mat.bothFace);
			else
				drawContext.BindDepthOnlyPipeline(mat.bothFace);
			commandList->SetGraphicsRootConstantBufferView(
				1, pixelConstantBuffer.ResolveGpuAddress() + sceneSurfaceConstantOffset);
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
	}

	Dx12Drawer::Dx12Drawer(const Dx12Instance& sourceInstance, Dx12ModelResources& sourceResources,
		const Dx12DrawContext& sourceDrawContext, Viewer& sourceViewer)
		: Drawer(sourceViewer), instance(sourceInstance), resources(sourceResources),
		drawContext(sourceDrawContext) {}
}
