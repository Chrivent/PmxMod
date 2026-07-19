#include "Viewer/Drawer/Dx12Drawer.h"

#include "Viewer/DrawContext/Dx12DrawContext.h"
#include "Viewer/Instance/Dx12Instance.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"

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

	GraphicsError::Result<void> Dx12Drawer::DrawModel() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX12 모델 패스 기록", "활성 command list가 없습니다"));
		}
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		if (!constantBuffer.Write(vertexConstants, layout.modelVertex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX12 모델 패스 기록", "모델 vertex 상수를 업로드하지 못했습니다"));
		ID3D12DescriptorHeap* descriptorHeaps[] = { resources.textureDescriptorHeap.Get() };
		if (descriptorHeaps[0] != nullptr)
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		drawContext.BindModelRootSignature();
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
				drawState, mat, base, toon, sphere);
			const size_t materialOffset = layout.materialBase + materialId * layout.materialStride;
			if (!constantBuffer.Write(pixelConstants, materialOffset + layout.modelPixel))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"DX12 모델 패스 기록", "모델 pixel 상수를 업로드하지 못했습니다"));
			commandList->SetGraphicsRootConstantBufferView(
				1, constantBuffer.GetGpuAddress() + materialOffset + layout.modelPixel);
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return {};
	}

	GraphicsError::Result<void> Dx12Drawer::DrawEdge() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX12 엣지 패스 기록", "활성 command list가 없습니다"));
		}
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		EdgeVertexConstants vertexConstants = BuildEdgeVertexConstants(
			drawState, world, ClipMatrix(), drawState.screenSize);
		drawContext.BindEdgePipeline();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& mat = resources.materials[materialId].material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			vertexConstants.edgeSize = mat.edgeSize;
			const size_t materialOffset = layout.materialBase + materialId * layout.materialStride;
			if (!constantBuffer.Write(vertexConstants, materialOffset + layout.edgeVertex))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"DX12 엣지 패스 기록", "엣지 vertex 상수를 업로드하지 못했습니다"));
			EdgePixelConstants pixelConstants{};
			pixelConstants.edgeColor = mat.edgeColor;
			if (!constantBuffer.Write(pixelConstants, materialOffset + layout.edgePixel))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"DX12 엣지 패스 기록", "엣지 pixel 상수를 업로드하지 못했습니다"));
			commandList->SetGraphicsRootConstantBufferView(
				0, constantBuffer.GetGpuAddress() + materialOffset + layout.edgeVertex);
			commandList->SetGraphicsRootConstantBufferView(
				1, constantBuffer.GetGpuAddress() + materialOffset + layout.edgePixel);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return {};
	}

	GraphicsError::Result<void> Dx12Drawer::DrawGroundShadow() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX12 지면 그림자 패스 기록", "활성 command list가 없습니다"));
		}
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			drawState, world, ClipMatrix());
		if (!constantBuffer.Write(vertexConstants, layout.groundShadowVertex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX12 지면 그림자 패스 기록", "지면 그림자 vertex 상수를 업로드하지 못했습니다"));
		constexpr GroundShadowPixelConstants pixelConstants{};
		if (!constantBuffer.Write(pixelConstants, layout.groundShadowPixel))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX12 지면 그림자 패스 기록", "지면 그림자 pixel 상수를 업로드하지 못했습니다"));
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
		return {};
	}

	GraphicsError::Result<void> Dx12Drawer::DrawSceneInputs() {
		ID3D12GraphicsCommandList* commandList = drawContext.TryGetCommandList();
		if (commandList == nullptr) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX12 후처리 장면 입력 기록", "활성 command list가 없습니다"));
		}
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const auto& vertexBufferView = resources.vertexBufferViews[frameIndex];
		const Dx12Buffer& constantBuffer = resources.constantBuffers[frameIndex];
		const auto& layout = resources.constantBufferLayout;
		const glm::mat4 world = BuildWorldMatrix(instance.GetScale());
		const SceneVelocityVertexConstants velocityConstants = BuildSceneVelocityVertexConstants(
			drawState, world, ClipMatrix());
		const ModelVertexConstants depthConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		const bool velocityRequired = drawState.velocityRequired;
		if (!(velocityRequired ? constantBuffer.Write(velocityConstants, layout.sceneInputVertex)
			: constantBuffer.Write(depthConstants, layout.sceneInputVertex)))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX12 후처리 장면 입력 기록", "장면 입력 vertex 상수를 업로드하지 못했습니다"));
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&resources.indexBufferView);
		drawContext.BindModelRootSignature();
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
			if (!constantBuffer.Write(pixelConstants, materialOffset + layout.sceneSurfacePixel))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"DX12 후처리 장면 입력 기록", "장면 표면 상수를 업로드하지 못했습니다"));
			if (velocityRequired)
				drawContext.BindSceneVelocityPipeline(mat.bothFace);
			else
				drawContext.BindSceneDepthPipeline(mat.bothFace);
			commandList->SetGraphicsRootConstantBufferView(
				1, constantBuffer.GetGpuAddress() + materialOffset + layout.sceneSurfacePixel);
			if (material.textureDescriptorHandle.ptr != 0)
				commandList->SetGraphicsRootDescriptorTable(2, material.textureDescriptorHandle);
			commandList->DrawIndexedInstanced(indexCount, 1, beginIndex, 0, 0);
		}
		return {};
	}

	Dx12Drawer::Dx12Drawer(const Dx12Instance& sourceInstance, Dx12ModelResources& sourceResources,
		Dx12DrawContext& sourceDrawContext)
		: Drawer(GraphicsApi::DirectX12), instance(sourceInstance),
		resources(sourceResources), drawContext(sourceDrawContext) {}
}
