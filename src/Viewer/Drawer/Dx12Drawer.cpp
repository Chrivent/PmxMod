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

	bool Dx12Drawer::DrawModel() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr)
			return false;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const auto& viewer = this->viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		if (!constantBuffer.Write(vertexConstants, layout.modelVertex)) {
			std::cerr << "Failed to update DX12 model vertex constants.\n";
			return false;
		}
		ID3D12DescriptorHeap* descriptorHeaps[] = { resources.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(
			0, constantBuffer.GetGpuAddress() + layout.modelVertex);
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
			const size_t materialOffset = layout.materialBase + materialId * layout.materialStride;
			if (!constantBuffer.Write(pixelConstants, materialOffset + layout.modelPixel)) {
				std::cerr << "Failed to update DX12 model pixel constants.\n";
				return false;
			}
			commandList->SetGraphicsRootConstantBufferView(
				1, constantBuffer.GetGpuAddress() + materialOffset + layout.modelPixel);
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return true;
	}

	bool Dx12Drawer::DrawEdge() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr)
			return false;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const auto& viewer = this->viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		EdgeVertexConstants vertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.GetScreenWidth(), viewer.GetScreenHeight()));
		drawContext.BindEdgePipeline();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& mat = resources.materials[materialId].material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			vertexConstants.edgeSize = mat.edgeSize;
			const size_t materialOffset = layout.materialBase + materialId * layout.materialStride;
			if (!constantBuffer.Write(vertexConstants, materialOffset + layout.edgeVertex)) {
				std::cerr << "Failed to update DX12 edge vertex constants.\n";
				return false;
			}
			EdgePixelConstants pixelConstants{};
			pixelConstants.edgeColor = mat.edgeColor;
			if (!constantBuffer.Write(pixelConstants, materialOffset + layout.edgePixel)) {
				std::cerr << "Failed to update DX12 edge pixel constants.\n";
				return false;
			}
			commandList->SetGraphicsRootConstantBufferView(
				0, constantBuffer.GetGpuAddress() + materialOffset + layout.edgeVertex);
			commandList->SetGraphicsRootConstantBufferView(
				1, constantBuffer.GetGpuAddress() + materialOffset + layout.edgePixel);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return true;
	}

	bool Dx12Drawer::DrawGroundShadow() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr)
			return false;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const auto& viewer = this->viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
		if (!constantBuffer.Write(vertexConstants, layout.groundShadowVertex)) {
			std::cerr << "Failed to update DX12 ground shadow vertex constants.\n";
			return false;
		}
		constexpr GroundShadowPixelConstants pixelConstants{};
		if (!constantBuffer.Write(pixelConstants, layout.groundShadowPixel)) {
			std::cerr << "Failed to update DX12 ground shadow pixel constants.\n";
			return false;
		}
		drawContext.BindGroundShadowPipeline();
		commandList->OMSetStencilRef(0x01);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(
			0, constantBuffer.GetGpuAddress() + layout.groundShadowVertex);
		commandList->SetGraphicsRootConstantBufferView(
			1, constantBuffer.GetGpuAddress() + layout.groundShadowPixel);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& mat = resources.materials[materialId].material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return true;
	}

	bool Dx12Drawer::DrawSceneInputs() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr)
			return false;
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const auto& viewer = this->viewer;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			viewer, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		const bool velocityRequired = viewer.RequiresPostProcessVelocity();
		if (!(velocityRequired ? constantBuffer.Write(velocityConstants, layout.sceneInputVertex)
			: constantBuffer.Write(depthConstants, layout.sceneInputVertex))) {
			std::cerr << "Failed to update DX12 scene input vertex constants.\n";
			return false;
		}
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(
			0, constantBuffer.GetGpuAddress() + layout.sceneInputVertex);
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
			const size_t materialOffset = layout.materialBase + materialId * layout.materialStride;
			if (!constantBuffer.Write(pixelConstants, materialOffset + layout.sceneSurfacePixel)) {
				std::cerr << "Failed to update DX12 scene surface constants.\n";
				return false;
			}
			if (velocityRequired)
				drawContext.BindSceneVelocityPipeline(mat.bothFace);
			else
				drawContext.BindDepthOnlyPipeline(mat.bothFace);
			commandList->SetGraphicsRootConstantBufferView(
				1, constantBuffer.GetGpuAddress() + materialOffset + layout.sceneSurfacePixel);
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return true;
	}

	Dx12Drawer::Dx12Drawer(const Dx12Instance& sourceInstance, Dx12ModelResources& sourceResources,
		const Dx12DrawContext& sourceDrawContext, Viewer& sourceViewer)
		: Drawer(sourceViewer), instance(sourceInstance), resources(sourceResources),
		drawContext(sourceDrawContext) {}
}
