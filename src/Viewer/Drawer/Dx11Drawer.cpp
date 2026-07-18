#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/Instance/Dx11Instance.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

namespace Chrivent {
	void Dx11Drawer::BindTexture(
		const UINT slot, const Dx11Texture& texture, ID3D11SamplerState* sampler,
		ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const {
		ID3D11ShaderResourceView* views = texture.texture
		? texture.textureView.Get() : drawContext.GetDummyTextureView();
		ID3D11SamplerState* samplers = texture.texture
		? sampler : drawContext.GetTextureSampler();
		if (lastView != views) {
			drawContext.GetDeviceContext()->PSSetShaderResources(slot, 1, &views);
			lastView = views;
		}
		if (lastSampler != samplers) {
			drawContext.GetDeviceContext()->PSSetSamplers(slot, 1, &samplers);
			lastSampler = samplers;
		}
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

	bool Dx11Drawer::DrawModel() {
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
		drawContext.GetDeviceContext()->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		drawContext.GetDeviceContext()->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		ID3D11ShaderResourceView* boundViews[3] = { nullptr, nullptr, nullptr };
		ID3D11SamplerState* boundSamplers[3] = { nullptr, nullptr, nullptr };
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
			BindTexture(0, material.texture, drawContext.GetTextureSampler(),
				boundViews[0], boundSamplers[0]);
			BindTexture(1, material.toonTexture, drawContext.GetToonTextureSampler(),
				boundViews[1], boundSamplers[1]);
			BindTexture(2, material.sphereTexture, drawContext.GetTextureSampler(),
				boundViews[2], boundSamplers[2]);
			drawContext.GetDeviceContext()->UpdateSubresource(
				psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			drawContext.GetDeviceContext()->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			ID3D11RasterizerState* targetRs = drawContext.ResolveModelRasterizerState(mat.bothFace);
			if (currentRs != targetRs) {
				drawContext.GetDeviceContext()->RSSetState(targetRs);
				currentRs = targetRs;
			}
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return true;
	}

	bool Dx11Drawer::DrawEdge() {
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
		drawContext.GetDeviceContext()->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			vsCb1.edgeSize = mat.edgeSize;
			drawContext.GetDeviceContext()->UpdateSubresource(
				edgeVsConstantBuffer.Get(), 0, nullptr, &vsCb1, 0, 0);
			EdgePixelConstants psCb{};
			psCb.edgeColor = mat.edgeColor;
			drawContext.GetDeviceContext()->UpdateSubresource(
				edgePsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			drawContext.GetDeviceContext()->PSSetConstantBuffers(
				1, 1, edgePsConstantBuffer.GetAddressOf());
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return true;
	}

	bool Dx11Drawer::DrawGroundShadow() {
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
		drawContext.GetDeviceContext()->UpdateSubresource(
			gsVsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		drawContext.GetDeviceContext()->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		constexpr GroundShadowPixelConstants psCb;
		drawContext.GetDeviceContext()->UpdateSubresource(
			gsPsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
		drawContext.GetDeviceContext()->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return true;
	}

	bool Dx11Drawer::DrawSceneInputs() {
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
			drawContext.GetDeviceContext()->UpdateSubresource(
				vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		} else {
			const ModelVertexConstants vsCb = BuildModelVertexConstants(drawState, world, ClipMatrix());
			drawContext.GetDeviceContext()->UpdateSubresource(
				vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		}
		drawContext.BindSceneInputPipeline(velocityRequired);
		drawContext.GetDeviceContext()->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceContext()->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		drawContext.GetDeviceContext()->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceContext()->PSSetConstantBuffers(
			1, 1, sceneSurfaceConstantBuffer.GetAddressOf());
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.texture && material.texture.hasAlpha);
			drawContext.GetDeviceContext()->UpdateSubresource(
				sceneSurfaceConstantBuffer.Get(), 0, nullptr, &pixelConstants, 0, 0);
			ID3D11ShaderResourceView* baseTexture = material.texture.texture
				? material.texture.textureView.Get() : drawContext.GetDummyTextureView();
			ID3D11SamplerState* baseSampler = drawContext.GetTextureSampler();
			drawContext.GetDeviceContext()->PSSetShaderResources(0, 1, &baseTexture);
			drawContext.GetDeviceContext()->PSSetSamplers(0, 1, &baseSampler);
			ID3D11RasterizerState* targetRs = drawContext.ResolveModelRasterizerState(mat.bothFace);
			if (currentRs != targetRs) {
				drawContext.GetDeviceContext()->RSSetState(targetRs);
				currentRs = targetRs;
			}
			drawContext.GetDeviceContext()->DrawIndexed(indexCount, beginIndex, 0);
		}
		return true;
	}

	Dx11Drawer::Dx11Drawer(const Dx11Instance& sourceInstance, Dx11ModelResources& sourceResources,
		Dx11DrawContext& sourceDrawContext)
		: instance(sourceInstance), resources(sourceResources), drawContext(sourceDrawContext) {}
}
