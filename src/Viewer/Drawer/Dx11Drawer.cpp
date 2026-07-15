#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/Instance/Dx11Instance.h"
#include "Viewer/Viewer/Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

namespace Chrivent {
	void Dx11Drawer::BindTexture(
		const UINT slot, const Dx11Texture& texture, ID3D11SamplerState* sampler,
		ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const {
		ID3D11ShaderResourceView* views = texture.texture
		? texture.textureView.Get() : drawContext.GetDummyTexture().textureView.Get();
		ID3D11SamplerState* samplers = texture.texture
		? sampler : drawContext.GetPipelineStates().textureSampler.Get();
		if (lastView != views) {
			drawContext.GetDeviceResources().context->PSSetShaderResources(slot, 1, &views);
			lastView = views;
		}
		if (lastSampler != samplers) {
			drawContext.GetDeviceResources().context->PSSetSamplers(slot, 1, &samplers);
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

	void Dx11Drawer::DrawModel() {
		const auto& viewer = this->viewer;
		const auto& materials = resources.materials;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& vsConstantBuffer = resources.vsConstantBuffer;
		const auto& psConstantBuffer = resources.psConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.GetDeviceResources().context->OMSetDepthStencilState(
			drawContext.GetPipelineStates().defaultDss.Get(), 0x00);
		drawContext.GetDeviceResources().context->OMSetBlendState(
			drawContext.GetPipelineStates().blendState.Get(), nullptr, 0xffffffff);
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		drawContext.GetDeviceResources().context->IASetInputLayout(drawContext.GetShaders().model.inputLayout.Get());
		drawContext.GetDeviceResources().context->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const ModelVertexConstants vsCb = BuildModelVertexConstants(viewer, world, ClipMatrix());
		drawContext.GetDeviceResources().context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		drawContext.GetDeviceResources().context->VSSetShader(
			drawContext.GetShaders().model.vertexShader.Get(), nullptr, 0);
		drawContext.GetDeviceResources().context->PSSetShader(
			drawContext.GetShaders().model.pixelShader.Get(), nullptr, 0);
		drawContext.GetDeviceResources().context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		ID3D11ShaderResourceView* boundViews[3] = { nullptr, nullptr, nullptr };
		ID3D11SamplerState* boundSamplers[3] = { nullptr, nullptr, nullptr };
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (mat.diffuse.a == 0)
				continue;
			const int textureMode = material.texture.texture ? material.texture.hasAlpha ? 2 : 1 : 0;
			const int toonTextureMode = material.toonTexture.texture ? 1 : 0;
			int spMode = 0;
			if (material.sphereTexture.texture) {
				if (mat.spTextureMode == SphereMode::Mul)
					spMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					spMode = 2;
			}
			const ModelPixelConstants psCb = BuildModelPixelConstants(
				viewer, mat, textureMode, toonTextureMode, spMode);
			BindTexture(0, material.texture, drawContext.GetPipelineStates().textureSampler.Get(),
				boundViews[0], boundSamplers[0]);
			BindTexture(1, material.toonTexture, drawContext.GetPipelineStates().toonTextureSampler.Get(),
				boundViews[1], boundSamplers[1]);
			BindTexture(2, material.sphereTexture, drawContext.GetPipelineStates().textureSampler.Get(),
				boundViews[2], boundSamplers[2]);
			drawContext.GetDeviceResources().context->UpdateSubresource(
				psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			drawContext.GetDeviceResources().context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			ID3D11RasterizerState* targetRs = mat.bothFace
				? drawContext.GetPipelineStates().bothFaceRs.Get()
				: drawContext.GetPipelineStates().frontFaceRs.Get();
			if (currentRs != targetRs) {
				drawContext.GetDeviceResources().context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			drawContext.GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawEdge() {
		const auto& viewer = this->viewer;
		const auto& materials = resources.materials;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& edgeVsConstantBuffer = resources.edgeVsConstantBuffer;
		const auto& edgePsConstantBuffer = resources.edgePsConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.GetDeviceResources().context->IASetInputLayout(drawContext.GetShaders().edge.inputLayout.Get());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		drawContext.GetDeviceResources().context->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		EdgeVertexConstants vsCb1 = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.screenWidth, viewer.screenHeight));
		drawContext.GetDeviceResources().context->VSSetShader(
			drawContext.GetShaders().edge.vertexShader.Get(), nullptr, 0);
		drawContext.GetDeviceResources().context->PSSetShader(
			drawContext.GetShaders().edge.pixelShader.Get(), nullptr, 0);
		drawContext.GetDeviceResources().context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceResources().context->RSSetState(drawContext.GetPipelineStates().edgeRs.Get());
		drawContext.GetDeviceResources().context->OMSetDepthStencilState(
			drawContext.GetPipelineStates().defaultDss.Get(), 0x00);
		drawContext.GetDeviceResources().context->OMSetBlendState(
			drawContext.GetPipelineStates().blendState.Get(), nullptr, 0xffffffff);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			vsCb1.edgeSize = mat.edgeSize;
			drawContext.GetDeviceResources().context->UpdateSubresource(
				edgeVsConstantBuffer.Get(), 0, nullptr, &vsCb1, 0, 0);
			EdgePixelConstants psCb{};
			psCb.edgeColor = mat.edgeColor;
			drawContext.GetDeviceResources().context->UpdateSubresource(
				edgePsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			drawContext.GetDeviceResources().context->PSSetConstantBuffers(
				1, 1, edgePsConstantBuffer.GetAddressOf());
			drawContext.GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawGroundShadow() {
		const auto& viewer = this->viewer;
		const auto& materials = resources.materials;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& gsVsConstantBuffer = resources.gsVsConstantBuffer;
		const auto& gsPsConstantBuffer = resources.gsPsConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.GetDeviceResources().context->IASetInputLayout(
			drawContext.GetShaders().groundShadow.inputLayout.Get());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		drawContext.GetDeviceResources().context->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const GroundShadowVertexConstants vsCb = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
		drawContext.GetDeviceResources().context->UpdateSubresource(
			gsVsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		drawContext.GetDeviceResources().context->VSSetShader(
			drawContext.GetShaders().groundShadow.vertexShader.Get(), nullptr, 0);
		drawContext.GetDeviceResources().context->PSSetShader(
			drawContext.GetShaders().groundShadow.pixelShader.Get(), nullptr, 0);
		drawContext.GetDeviceResources().context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceResources().context->RSSetState(drawContext.GetPipelineStates().gsRs.Get());
		drawContext.GetDeviceResources().context->OMSetDepthStencilState(
			drawContext.GetPipelineStates().gsDss.Get(), 0x01);
		constexpr GroundShadowPixelConstants psCb;
		drawContext.GetDeviceResources().context->UpdateSubresource(
			gsPsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
		drawContext.GetDeviceResources().context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			drawContext.GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawSceneInputs() {
		const auto& viewer = this->viewer;
		const auto& vertexBuffer = resources.vertexBuffer;
		const auto& indexBuffer = resources.indexBuffer;
		const auto indexBufferFormat = resources.indexBufferFormat;
		const auto& vsConstantBuffer = resources.vsConstantBuffer;
		const auto& sceneSurfaceConstantBuffer = resources.sceneSurfaceConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		if (viewer.RequiresPostProcessVelocity()) {
			const SceneVelocityVertexConstants vsCb = BuildSceneVelocityVertexConstants(
				viewer, world, ClipMatrix());
			drawContext.GetDeviceResources().context->UpdateSubresource(
				vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
			drawContext.GetDeviceResources().context->IASetInputLayout(
				drawContext.GetShaders().sceneVelocity.inputLayout.Get());
			drawContext.GetDeviceResources().context->VSSetShader(
				drawContext.GetShaders().sceneVelocity.vertexShader.Get(), nullptr, 0);
			drawContext.GetDeviceResources().context->PSSetShader(
				drawContext.GetShaders().sceneVelocity.pixelShader.Get(), nullptr, 0);
		} else {
			const ModelVertexConstants vsCb = BuildModelVertexConstants(viewer, world, ClipMatrix());
			drawContext.GetDeviceResources().context->UpdateSubresource(
				vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
			drawContext.GetDeviceResources().context->IASetInputLayout(
				drawContext.GetShaders().sceneDepth.inputLayout.Get());
			drawContext.GetDeviceResources().context->VSSetShader(
				drawContext.GetShaders().sceneDepth.vertexShader.Get(), nullptr, 0);
			drawContext.GetDeviceResources().context->PSSetShader(
				drawContext.GetShaders().sceneDepth.pixelShader.Get(), nullptr, 0);
		}
		drawContext.GetDeviceResources().context->IASetVertexBuffers(
			0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		drawContext.GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		drawContext.GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		drawContext.GetDeviceResources().context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		drawContext.GetDeviceResources().context->PSSetConstantBuffers(
			1, 1, sceneSurfaceConstantBuffer.GetAddressOf());
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.texture && material.texture.hasAlpha);
			drawContext.GetDeviceResources().context->UpdateSubresource(
				sceneSurfaceConstantBuffer.Get(), 0, nullptr, &pixelConstants, 0, 0);
			ID3D11ShaderResourceView* baseTexture = material.texture.texture
				? material.texture.textureView.Get() : drawContext.GetDummyTexture().textureView.Get();
			ID3D11SamplerState* baseSampler = drawContext.GetPipelineStates().textureSampler.Get();
			drawContext.GetDeviceResources().context->PSSetShaderResources(0, 1, &baseTexture);
			drawContext.GetDeviceResources().context->PSSetSamplers(0, 1, &baseSampler);
			ID3D11RasterizerState* targetRs = mat.bothFace
				? drawContext.GetPipelineStates().bothFaceRs.Get()
				: drawContext.GetPipelineStates().frontFaceRs.Get();
			if (currentRs != targetRs) {
				drawContext.GetDeviceResources().context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			drawContext.GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	Dx11Drawer::Dx11Drawer(const Dx11Instance& sourceInstance, Dx11ModelResources& sourceResources,
		const Dx11DrawContext& sourceDrawContext, Viewer& sourceViewer)
		: Drawer(sourceViewer), instance(sourceInstance), resources(sourceResources),
		drawContext(sourceDrawContext) {}
}
