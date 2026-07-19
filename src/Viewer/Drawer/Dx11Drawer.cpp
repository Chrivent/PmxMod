#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/Instance/Dx11Instance.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

namespace Chrivent {
	void Dx11Drawer::BindTexture(const UINT textureSlot, const UINT samplerSlot,
		const Dx11Texture& texture, ID3D11SamplerState* sampler,
		ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const {
		ID3D11ShaderResourceView* views = texture.texture
		? texture.textureView.Get() : drawContext.GetDummyTextureView();
		ID3D11SamplerState* samplers = texture.texture
		? sampler : drawContext.GetTextureSampler();
		if (lastView != views) {
			drawContext.GetDeviceContext()->PSSetShaderResources(textureSlot, 1, &views);
			lastView = views;
		}
		if (lastSampler != samplers) {
			drawContext.GetDeviceContext()->PSSetSamplers(samplerSlot, 1, &samplers);
			lastSampler = samplers;
		}
	}

	GraphicsResult<void> Dx11Drawer::WriteConstantBuffer(ID3D11Buffer* buffer,
		const void* data, const size_t size, const char* operation) const {
		if (buffer == nullptr || data == nullptr || size == 0) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				operation, "DirectX 11 상수 버퍼 입력이 올바르지 않습니다"));
		}
		D3D11_MAPPED_SUBRESOURCE mappedResource{};
		const HRESULT result = drawContext.GetDeviceContext()->Map(
			buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(result)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				operation, "DirectX 11 상수 버퍼를 매핑하지 못했습니다", result, true));
		}
		std::memcpy(mappedResource.pData, data, size);
		drawContext.GetDeviceContext()->Unmap(buffer, 0);
		return {};
	}

	const glm::mat4& Dx11Drawer::ClipMatrix() const {
		static constexpr glm::mat4 clipMatrix(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return clipMatrix;
	}

	GraphicsResult<void> Dx11Drawer::DrawModel() {
		const auto& materials = resources.materials;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& vsConstantBuffer = resources.vsConstantBuffer;
		const auto& psConstantBuffer = resources.psConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.BindModelPipeline();
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		drawContext.GetDeviceContext()->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceContext()->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const ModelVertexConstants vsCb = BuildModelVertexConstants(drawState, world, ClipMatrix());
		const auto vertexWriteResult = WriteConstantBuffer(
			vsConstantBuffer.Get(), &vsCb, sizeof(vsCb), "모델 vertex 상수 기록");
		if (!vertexWriteResult)
			return std::unexpected(vertexWriteResult.error());
		drawContext.GetDeviceContext()->VSSetConstantBuffers(
			SceneShaderInputLayout::vertexConstantRegister, 1, vsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceContext()->PSSetConstantBuffers(
			SceneShaderInputLayout::pixelConstantRegister, 1, psConstantBuffer.GetAddressOf());
		ID3D11ShaderResourceView* boundViews[SceneShaderInputLayout::textureCount]{};
		ID3D11SamplerState* boundSamplers[SceneShaderInputLayout::samplerCount]{};
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawModelMaterial(mat))
				continue;
			const auto [base, toon, sphere] = ResolveMaterialTextureModes(mat,
				material.texture.texture != nullptr, material.texture.hasAlpha,
				material.toonTexture.texture != nullptr, material.sphereTexture.texture != nullptr);
			const ModelPixelConstants psCb = BuildModelPixelConstants(
				drawState, mat, base, toon, sphere);
			BindTexture(SceneShaderInputLayout::baseTextureRegister,
				SceneShaderInputLayout::baseSamplerRegister,
				material.texture, drawContext.GetTextureSampler(),
				boundViews[SceneShaderInputLayout::baseTextureRegister],
				boundSamplers[SceneShaderInputLayout::baseSamplerRegister]);
			BindTexture(SceneShaderInputLayout::toonTextureRegister,
				SceneShaderInputLayout::toonSamplerRegister,
				material.toonTexture, drawContext.GetToonTextureSampler(),
				boundViews[SceneShaderInputLayout::toonTextureRegister],
				boundSamplers[SceneShaderInputLayout::toonSamplerRegister]);
			BindTexture(SceneShaderInputLayout::sphereTextureRegister,
				SceneShaderInputLayout::sphereSamplerRegister,
				material.sphereTexture, drawContext.GetTextureSampler(),
				boundViews[SceneShaderInputLayout::sphereTextureRegister],
				boundSamplers[SceneShaderInputLayout::sphereSamplerRegister]);
			const auto pixelWriteResult = WriteConstantBuffer(
				psConstantBuffer.Get(), &psCb, sizeof(psCb), "모델 pixel 상수 기록");
			if (!pixelWriteResult)
				return std::unexpected(pixelWriteResult.error());
			ID3D11RasterizerState* targetRs = drawContext.ResolveModelRasterizerState(mat.bothFace);
			if (currentRs != targetRs) {
				drawContext.GetDeviceContext()->RSSetState(targetRs);
				currentRs = targetRs;
			}
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return {};
	}

	GraphicsResult<void> Dx11Drawer::DrawEdge() {
		const auto& materials = resources.materials;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& edgeVsConstantBuffer = resources.edgeVsConstantBuffer;
		const auto& edgePsConstantBuffer = resources.edgePsConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.BindEdgePipeline();
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		drawContext.GetDeviceContext()->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceContext()->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		EdgeVertexConstants vsCb1 = BuildEdgeVertexConstants(
			drawState, world, ClipMatrix(), drawState.screenSize);
		drawContext.GetDeviceContext()->VSSetConstantBuffers(
			SceneShaderInputLayout::vertexConstantRegister, 1, edgeVsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceContext()->PSSetConstantBuffers(
			SceneShaderInputLayout::pixelConstantRegister, 1, edgePsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			vsCb1.edgeSize = mat.edgeSize;
			const auto vertexWriteResult = WriteConstantBuffer(
				edgeVsConstantBuffer.Get(), &vsCb1, sizeof(vsCb1), "엣지 vertex 상수 기록");
			if (!vertexWriteResult)
				return std::unexpected(vertexWriteResult.error());
			EdgePixelConstants psCb{};
			psCb.edgeColor = mat.edgeColor;
			const auto pixelWriteResult = WriteConstantBuffer(
				edgePsConstantBuffer.Get(), &psCb, sizeof(psCb), "엣지 pixel 상수 기록");
			if (!pixelWriteResult)
				return std::unexpected(pixelWriteResult.error());
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return {};
	}

	GraphicsResult<void> Dx11Drawer::DrawGroundShadow() {
		const auto& materials = resources.materials;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& gsVsConstantBuffer = resources.gsVsConstantBuffer;
		const auto& gsPsConstantBuffer = resources.gsPsConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.BindGroundShadowPipeline();
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		drawContext.GetDeviceContext()->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceContext()->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const GroundShadowVertexConstants vsCb = BuildGroundShadowVertexConstants(
			drawState, world, ClipMatrix());
		const auto vertexWriteResult = WriteConstantBuffer(
			gsVsConstantBuffer.Get(), &vsCb, sizeof(vsCb), "지면 그림자 vertex 상수 기록");
		if (!vertexWriteResult)
			return std::unexpected(vertexWriteResult.error());
		drawContext.GetDeviceContext()->VSSetConstantBuffers(
			SceneShaderInputLayout::vertexConstantRegister, 1, gsVsConstantBuffer.GetAddressOf());
		constexpr GroundShadowPixelConstants psCb;
		const auto pixelWriteResult = WriteConstantBuffer(
			gsPsConstantBuffer.Get(), &psCb, sizeof(psCb), "지면 그림자 pixel 상수 기록");
		if (!pixelWriteResult)
			return std::unexpected(pixelWriteResult.error());
		drawContext.GetDeviceContext()->PSSetConstantBuffers(
			SceneShaderInputLayout::pixelConstantRegister, 1, gsPsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return {};
	}

	GraphicsResult<void> Dx11Drawer::DrawSceneInputs() {
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& vsConstantBuffer = resources.vsConstantBuffer;
		const auto& sceneSurfaceConstantBuffer = resources.sceneSurfaceConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const bool velocityRequired = drawState.velocityRequired;
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		if (velocityRequired) {
			const SceneVelocityVertexConstants vsCb = BuildSceneVelocityVertexConstants(
				drawState, world, ClipMatrix());
			const auto writeResult = WriteConstantBuffer(
				vsConstantBuffer.Get(), &vsCb, sizeof(vsCb), "장면 velocity vertex 상수 기록");
			if (!writeResult)
				return std::unexpected(writeResult.error());
		} else {
			const ModelVertexConstants vsCb = BuildModelVertexConstants(drawState, world, ClipMatrix());
			const auto writeResult = WriteConstantBuffer(
				vsConstantBuffer.Get(), &vsCb, sizeof(vsCb), "장면 depth vertex 상수 기록");
			if (!writeResult)
				return std::unexpected(writeResult.error());
		}
		if (velocityRequired)
			drawContext.BindSceneVelocityPipeline();
		else
			drawContext.BindSceneDepthPipeline();
		drawContext.GetDeviceContext()->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceContext()->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		drawContext.GetDeviceContext()->VSSetConstantBuffers(
			SceneShaderInputLayout::vertexConstantRegister, 1, vsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceContext()->PSSetConstantBuffers(
			SceneShaderInputLayout::pixelConstantRegister, 1,
			sceneSurfaceConstantBuffer.GetAddressOf());
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.texture && material.texture.hasAlpha);
			const auto writeResult = WriteConstantBuffer(sceneSurfaceConstantBuffer.Get(),
				&pixelConstants, sizeof(pixelConstants), "장면 입력 pixel 상수 기록");
			if (!writeResult)
				return std::unexpected(writeResult.error());
			ID3D11ShaderResourceView* baseTexture = material.texture.texture
				? material.texture.textureView.Get() : drawContext.GetDummyTextureView();
			ID3D11SamplerState* baseSampler = drawContext.GetTextureSampler();
			drawContext.GetDeviceContext()->PSSetShaderResources(
				SceneShaderInputLayout::baseTextureRegister, 1, &baseTexture);
			drawContext.GetDeviceContext()->PSSetSamplers(
				SceneShaderInputLayout::baseSamplerRegister, 1, &baseSampler);
			ID3D11RasterizerState* targetRs = drawContext.ResolveModelRasterizerState(mat.bothFace);
			if (currentRs != targetRs) {
				drawContext.GetDeviceContext()->RSSetState(targetRs);
				currentRs = targetRs;
			}
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return {};
	}

	Dx11Drawer::Dx11Drawer(const Dx11Instance& sourceInstance, Dx11ModelResources& sourceResources,
		Dx11DrawContext& sourceDrawContext)
		: Drawer(GraphicsApi::DirectX11), instance(sourceInstance),
		resources(sourceResources), drawContext(sourceDrawContext) {}
}
