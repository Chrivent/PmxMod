#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/Instance/Dx11Instance.h"
#include "Viewer/Viewer/Dx11Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

namespace Chrivent {
	void Dx11Drawer::BindTexture(
		const UINT slot, const Dx11Texture& texture, ID3D11SamplerState* sampler,
		const int modeIfPresent, int& mode, glm::vec4& mulFactor, glm::vec4& addFactor,
		const glm::vec4& sourceMulFactor, const glm::vec4& sourceAddFactor,
		ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const {
		if (texture.texture) {
			mode = modeIfPresent;
			mulFactor  = sourceMulFactor;
			addFactor  = sourceAddFactor;
		} else
			mode = 0;
		ID3D11ShaderResourceView* views = texture.texture
		? texture.textureView.Get() : instance.viewer->dummyTexture.textureView.Get();
		ID3D11SamplerState* samplers = texture.texture
		? sampler : instance.viewer->pipelineStates.textureSampler.Get();
		if (lastView != views) {
			instance.viewer->deviceResources.context->PSSetShaderResources(slot, 1, &views);
			lastView = views;
		}
		if (lastSampler != samplers) {
			instance.viewer->deviceResources.context->PSSetSamplers(slot, 1, &samplers);
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
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const auto lightDir = glm::mat3(viewer->viewMat) * viewer->lightDir;
		const auto wv = view * world;
		const auto wvp = ClipMatrix() * proj * view * world;
		ModelPixelConstants basePsCb{};
		basePsCb.lightColor = glm::vec4(viewer->lightColor, 0.0f);
		basePsCb.lightDir = glm::vec4(lightDir, 0.0f);
		viewer->deviceResources.context->OMSetDepthStencilState(viewer->pipelineStates.defaultDss.Get(), 0x00);
		viewer->deviceResources.context->OMSetBlendState(viewer->pipelineStates.blendState.Get(), nullptr, 0xffffffff);
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		viewer->deviceResources.context->IASetInputLayout(viewer->shaders.model.inputLayout.Get());
		viewer->deviceResources.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ModelVertexConstants vsCb;
		vsCb.wv = wv;
		vsCb.wvp = wvp;
		viewer->deviceResources.context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->deviceResources.context->VSSetShader(viewer->shaders.model.vertexShader.Get(), nullptr, 0);
		viewer->deviceResources.context->PSSetShader(viewer->shaders.model.pixelShader.Get(), nullptr, 0);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		ID3D11ShaderResourceView* boundViews[3] = { nullptr, nullptr, nullptr };
		ID3D11SamplerState* boundSamplers[3] = { nullptr, nullptr, nullptr };
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants psCb = basePsCb;
			psCb.diffuseAlpha = mat.diffuse;
			psCb.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			psCb.specular = glm::vec4(mat.specular, 0.0f);
			int baseMode = 0;
			if (material.texture.texture)
				baseMode = !material.texture.hasAlpha ? 1 : 2;
			BindTexture(
				0, material.texture, viewer->pipelineStates.textureSampler.Get(), baseMode,
				psCb.textureModes.x, psCb.texMulFactor, psCb.texAddFactor, mat.textureMulFactor, mat.textureAddFactor,
				boundViews[0], boundSamplers[0]
			);
			BindTexture(
				1, material.toonTexture, viewer->pipelineStates.toonTextureSampler.Get(), 1,
				psCb.textureModes.y, psCb.toonTexMulFactor, psCb.toonTexAddFactor, mat.toonTextureMulFactor, mat.toonTextureAddFactor,
				boundViews[1], boundSamplers[1]
			);
			int spMode = 0;
			if (material.sphereTexture.texture) {
				if (mat.spTextureMode == SphereMode::Mul)
					spMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					spMode = 2;
			}
			BindTexture(
				2, material.sphereTexture, viewer->pipelineStates.textureSampler.Get(), spMode,
				psCb.textureModes.z, psCb.sphereTexMulFactor, psCb.sphereTexAddFactor, mat.sphereTextureMulFactor, mat.sphereTextureAddFactor,
				boundViews[2], boundSamplers[2]
			);
			viewer->deviceResources.context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->deviceResources.context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			ID3D11RasterizerState* targetRs = mat.bothFace
				? viewer->pipelineStates.bothFaceRs.Get()
				: viewer->pipelineStates.frontFaceRs.Get();
			if (currentRs != targetRs) {
				viewer->deviceResources.context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
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
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const auto wv = view * world;
		const auto wvp = ClipMatrix() * proj * view * world;
		viewer->deviceResources.context->IASetInputLayout(viewer->shaders.edge.inputLayout.Get());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		viewer->deviceResources.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		EdgeVertexConstants vsCb1{};
		vsCb1.wv = wv;
		vsCb1.wvp = wvp;
		vsCb1.screenSize = glm::vec2(viewer->screenWidth, viewer->screenHeight);
		viewer->deviceResources.context->VSSetShader(viewer->shaders.edge.vertexShader.Get(), nullptr, 0);
		viewer->deviceResources.context->PSSetShader(viewer->shaders.edge.pixelShader.Get(), nullptr, 0);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		viewer->deviceResources.context->RSSetState(viewer->pipelineStates.edgeRs.Get());
		viewer->deviceResources.context->OMSetDepthStencilState(viewer->pipelineStates.defaultDss.Get(), 0x00);
		viewer->deviceResources.context->OMSetBlendState(viewer->pipelineStates.blendState.Get(), nullptr, 0xffffffff);
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			vsCb1.edgeSize = mat.edgeSize;
			viewer->deviceResources.context->UpdateSubresource(edgeVsConstantBuffer.Get(), 0, nullptr, &vsCb1, 0, 0);
			EdgePixelConstants psCb{};
			psCb.edgeColor = mat.edgeColor;
			viewer->deviceResources.context->UpdateSubresource(edgePsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->deviceResources.context->PSSetConstantBuffers(1, 1, edgePsConstantBuffer.GetAddressOf());
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
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
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		viewer->deviceResources.context->IASetInputLayout(viewer->shaders.groundShadow.inputLayout.Get());
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		viewer->deviceResources.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const glm::mat4 shadow = BuildGroundShadowMatrix(viewer->lightDir);
		GroundShadowVertexConstants vsCb;
		vsCb.wvp = ClipMatrix() * proj * view * shadow * world;
		viewer->deviceResources.context->UpdateSubresource(gsVsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->deviceResources.context->VSSetShader(viewer->shaders.groundShadow.vertexShader.Get(), nullptr, 0);
		viewer->deviceResources.context->PSSetShader(viewer->shaders.groundShadow.pixelShader.Get(), nullptr, 0);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		viewer->deviceResources.context->RSSetState(viewer->pipelineStates.gsRs.Get());
		viewer->deviceResources.context->OMSetDepthStencilState(viewer->pipelineStates.gsDss.Get(), 0x01);
		GroundShadowPixelConstants psCb;
		psCb.shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
		viewer->deviceResources.context->UpdateSubresource(gsPsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
		viewer->deviceResources.context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawDepthOnly() {
		const auto* viewer = instance.viewer;
		const auto& vertexBuffer = instance.vertexBuffer;
		const auto& indexBuffer = instance.indexBuffer;
		const auto indexBufferFormat = instance.indexBufferFormat;
		const auto& vsConstantBuffer = instance.vsConstantBuffer;
		const auto& sceneSurfaceConstantBuffer = instance.sceneSurfaceConstantBuffer;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		constexpr UINT stride = sizeof(ViewerVertex);
		constexpr UINT offset = 0;
		if (viewer->RequiresPostProcessVelocity()) {
			SceneVelocityVertexConstants vsCb;
			vsCb.currentWvp = ClipMatrix() * proj * view * world;
			vsCb.previousWvp = viewer->postProcessHistoryResetPending ? vsCb.currentWvp
				: ClipMatrix() * viewer->previousProjMat * viewer->previousViewMat * world;
			viewer->deviceResources.context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
			viewer->deviceResources.context->IASetInputLayout(viewer->shaders.sceneVelocity.inputLayout.Get());
			viewer->deviceResources.context->VSSetShader(viewer->shaders.sceneVelocity.vertexShader.Get(), nullptr, 0);
			viewer->deviceResources.context->PSSetShader(viewer->shaders.sceneVelocity.pixelShader.Get(), nullptr, 0);
		} else {
			ModelVertexConstants vsCb;
			vsCb.wv = view * world;
			vsCb.wvp = ClipMatrix() * proj * view * world;
			viewer->deviceResources.context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
			viewer->deviceResources.context->IASetInputLayout(viewer->shaders.sceneDepth.inputLayout.Get());
			viewer->deviceResources.context->VSSetShader(viewer->shaders.sceneDepth.vertexShader.Get(), nullptr, 0);
			viewer->deviceResources.context->PSSetShader(viewer->shaders.sceneDepth.pixelShader.Get(), nullptr, 0);
		}
		viewer->deviceResources.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		viewer->deviceResources.context->PSSetConstantBuffers(1, 1, sceneSurfaceConstantBuffer.GetAddressOf());
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture.texture && material.texture.hasAlpha);
			viewer->deviceResources.context->UpdateSubresource(
				sceneSurfaceConstantBuffer.Get(), 0, nullptr, &pixelConstants, 0, 0);
			ID3D11ShaderResourceView* baseTexture = material.texture.texture
				? material.texture.textureView.Get() : viewer->dummyTexture.textureView.Get();
			ID3D11SamplerState* baseSampler = viewer->pipelineStates.textureSampler.Get();
			viewer->deviceResources.context->PSSetShaderResources(0, 1, &baseTexture);
			viewer->deviceResources.context->PSSetSamplers(0, 1, &baseSampler);
			ID3D11RasterizerState* targetRs = mat.bothFace
				? viewer->pipelineStates.bothFaceRs.Get()
				: viewer->pipelineStates.frontFaceRs.Get();
			if (currentRs != targetRs) {
				viewer->deviceResources.context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	Dx11Drawer::Dx11Drawer(const Dx11Instance& sourceInstance) : instance(sourceInstance) {}
}
