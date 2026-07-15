#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/Instance/Dx11Instance.h"
#include "Viewer/Viewer/Dx11Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

namespace Chrivent {
	void Dx11Drawer::BindTexture(
		const UINT slot, const Dx11Texture& texture, ID3D11SamplerState* sampler,
		ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const {
		ID3D11ShaderResourceView* views = texture.texture
		? texture.textureView.Get() : instance.viewer->GetDummyTexture().textureView.Get();
		ID3D11SamplerState* samplers = texture.texture
		? sampler : instance.viewer->GetPipelineStates().textureSampler.Get();
		if (lastView != views) {
			instance.viewer->GetDeviceResources().context->PSSetShaderResources(slot, 1, &views);
			lastView = views;
		}
		if (lastSampler != samplers) {
			instance.viewer->GetDeviceResources().context->PSSetSamplers(slot, 1, &samplers);
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
		const auto* viewer = instance.viewer;
		if (!viewer->modelEffectEnabled)
			return;
		const auto& materials = instance.materials;
		const auto& vertexBuffer = instance.vertexBuffer;
		const auto& indexBuffer = instance.indexBuffer;
		const auto indexBufferFormat = instance.indexBufferFormat;
		const auto& vsConstantBuffer = instance.vsConstantBuffer;
		const auto& psConstantBuffer = instance.psConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		viewer->GetDeviceResources().context->OMSetDepthStencilState(viewer->GetPipelineStates().defaultDss.Get(), 0x00);
		viewer->GetDeviceResources().context->OMSetBlendState(viewer->GetPipelineStates().blendState.Get(), nullptr, 0xffffffff);
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		viewer->GetDeviceResources().context->IASetInputLayout(viewer->GetShaders().model.inputLayout.Get());
		viewer->GetDeviceResources().context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const ModelVertexConstants vsCb = BuildModelVertexConstants(*viewer, world, ClipMatrix());
		viewer->GetDeviceResources().context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->GetDeviceResources().context->VSSetShader(viewer->GetShaders().model.vertexShader.Get(), nullptr, 0);
		viewer->GetDeviceResources().context->PSSetShader(viewer->GetShaders().model.pixelShader.Get(), nullptr, 0);
		viewer->GetDeviceResources().context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		ID3D11ShaderResourceView* boundViews[3] = { nullptr, nullptr, nullptr };
		ID3D11SamplerState* boundSamplers[3] = { nullptr, nullptr, nullptr };
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
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
				*viewer, mat, textureMode, toonTextureMode, spMode);
			BindTexture(0, material.texture, viewer->GetPipelineStates().textureSampler.Get(),
				boundViews[0], boundSamplers[0]);
			BindTexture(1, material.toonTexture, viewer->GetPipelineStates().toonTextureSampler.Get(),
				boundViews[1], boundSamplers[1]);
			BindTexture(2, material.sphereTexture, viewer->GetPipelineStates().textureSampler.Get(),
				boundViews[2], boundSamplers[2]);
			viewer->GetDeviceResources().context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDeviceResources().context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			ID3D11RasterizerState* targetRs = mat.bothFace
				? viewer->GetPipelineStates().bothFaceRs.Get()
				: viewer->GetPipelineStates().frontFaceRs.Get();
			if (currentRs != targetRs) {
				viewer->GetDeviceResources().context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			viewer->GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawEdge() {
		const auto* viewer = instance.viewer;
		if (!viewer->edgeEffectEnabled)
			return;
		const auto& materials = instance.materials;
		const auto& vertexBuffer = instance.vertexBuffer;
		const auto& indexBuffer = instance.indexBuffer;
		const auto indexBufferFormat = instance.indexBufferFormat;
		const auto& edgeVsConstantBuffer = instance.edgeVsConstantBuffer;
		const auto& edgePsConstantBuffer = instance.edgePsConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		viewer->GetDeviceResources().context->IASetInputLayout(viewer->GetShaders().edge.inputLayout.Get());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		viewer->GetDeviceResources().context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		EdgeVertexConstants vsCb1 = BuildEdgeVertexConstants(
			*viewer, world, ClipMatrix(), glm::vec2(viewer->screenWidth, viewer->screenHeight));
		viewer->GetDeviceResources().context->VSSetShader(viewer->GetShaders().edge.vertexShader.Get(), nullptr, 0);
		viewer->GetDeviceResources().context->PSSetShader(viewer->GetShaders().edge.pixelShader.Get(), nullptr, 0);
		viewer->GetDeviceResources().context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		viewer->GetDeviceResources().context->RSSetState(viewer->GetPipelineStates().edgeRs.Get());
		viewer->GetDeviceResources().context->OMSetDepthStencilState(viewer->GetPipelineStates().defaultDss.Get(), 0x00);
		viewer->GetDeviceResources().context->OMSetBlendState(viewer->GetPipelineStates().blendState.Get(), nullptr, 0xffffffff);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			vsCb1.edgeSize = mat.edgeSize;
			viewer->GetDeviceResources().context->UpdateSubresource(edgeVsConstantBuffer.Get(), 0, nullptr, &vsCb1, 0, 0);
			EdgePixelConstants psCb{};
			psCb.edgeColor = mat.edgeColor;
			viewer->GetDeviceResources().context->UpdateSubresource(edgePsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDeviceResources().context->PSSetConstantBuffers(1, 1, edgePsConstantBuffer.GetAddressOf());
			viewer->GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawGroundShadow() {
		const auto* viewer = instance.viewer;
		if (!viewer->groundShadowEffectEnabled)
			return;
		const auto& materials = instance.materials;
		const auto& vertexBuffer = instance.vertexBuffer;
		const auto& indexBuffer = instance.indexBuffer;
		const auto indexBufferFormat = instance.indexBufferFormat;
		const auto& gsVsConstantBuffer = instance.gsVsConstantBuffer;
		const auto& gsPsConstantBuffer = instance.gsPsConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		viewer->GetDeviceResources().context->IASetInputLayout(viewer->GetShaders().groundShadow.inputLayout.Get());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		viewer->GetDeviceResources().context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const GroundShadowVertexConstants vsCb = BuildGroundShadowVertexConstants(
			*viewer, world, ClipMatrix());
		viewer->GetDeviceResources().context->UpdateSubresource(gsVsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->GetDeviceResources().context->VSSetShader(viewer->GetShaders().groundShadow.vertexShader.Get(), nullptr, 0);
		viewer->GetDeviceResources().context->PSSetShader(viewer->GetShaders().groundShadow.pixelShader.Get(), nullptr, 0);
		viewer->GetDeviceResources().context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		viewer->GetDeviceResources().context->RSSetState(viewer->GetPipelineStates().gsRs.Get());
		viewer->GetDeviceResources().context->OMSetDepthStencilState(viewer->GetPipelineStates().gsDss.Get(), 0x01);
		constexpr GroundShadowPixelConstants psCb;
		viewer->GetDeviceResources().context->UpdateSubresource(gsPsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
		viewer->GetDeviceResources().context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			viewer->GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawSceneInputs() {
		const auto* viewer = instance.viewer;
		const auto& vertexBuffer = instance.vertexBuffer;
		const auto& indexBuffer = instance.indexBuffer;
		const auto indexBufferFormat = instance.indexBufferFormat;
		const auto& vsConstantBuffer = instance.vsConstantBuffer;
		const auto& sceneSurfaceConstantBuffer = instance.sceneSurfaceConstantBuffer;
		const auto world = BuildWorldMatrix(instance.GetScale());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		if (viewer->RequiresPostProcessVelocity()) {
			const SceneVelocityVertexConstants vsCb = BuildSceneVelocityVertexConstants(
				*viewer, world, ClipMatrix());
			viewer->GetDeviceResources().context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
			viewer->GetDeviceResources().context->IASetInputLayout(viewer->GetShaders().sceneVelocity.inputLayout.Get());
			viewer->GetDeviceResources().context->VSSetShader(viewer->GetShaders().sceneVelocity.vertexShader.Get(), nullptr, 0);
			viewer->GetDeviceResources().context->PSSetShader(viewer->GetShaders().sceneVelocity.pixelShader.Get(), nullptr, 0);
		} else {
			const ModelVertexConstants vsCb = BuildModelVertexConstants(*viewer, world, ClipMatrix());
			viewer->GetDeviceResources().context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
			viewer->GetDeviceResources().context->IASetInputLayout(viewer->GetShaders().sceneDepth.inputLayout.Get());
			viewer->GetDeviceResources().context->VSSetShader(viewer->GetShaders().sceneDepth.vertexShader.Get(), nullptr, 0);
			viewer->GetDeviceResources().context->PSSetShader(viewer->GetShaders().sceneDepth.pixelShader.Get(), nullptr, 0);
		}
		viewer->GetDeviceResources().context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->GetDeviceResources().context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->GetDeviceResources().context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		viewer->GetDeviceResources().context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		viewer->GetDeviceResources().context->PSSetConstantBuffers(1, 1, sceneSurfaceConstantBuffer.GetAddressOf());
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.texture && material.texture.hasAlpha);
			viewer->GetDeviceResources().context->UpdateSubresource(
				sceneSurfaceConstantBuffer.Get(), 0, nullptr, &pixelConstants, 0, 0);
			ID3D11ShaderResourceView* baseTexture = material.texture.texture
				? material.texture.textureView.Get() : viewer->GetDummyTexture().textureView.Get();
			ID3D11SamplerState* baseSampler = viewer->GetPipelineStates().textureSampler.Get();
			viewer->GetDeviceResources().context->PSSetShaderResources(0, 1, &baseTexture);
			viewer->GetDeviceResources().context->PSSetSamplers(0, 1, &baseSampler);
			ID3D11RasterizerState* targetRs = mat.bothFace
				? viewer->GetPipelineStates().bothFaceRs.Get()
				: viewer->GetPipelineStates().frontFaceRs.Get();
			if (currentRs != targetRs) {
				viewer->GetDeviceResources().context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			viewer->GetDeviceResources().context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	Dx11Drawer::Dx11Drawer(const Dx11Instance& sourceInstance) : instance(sourceInstance) {}
}
