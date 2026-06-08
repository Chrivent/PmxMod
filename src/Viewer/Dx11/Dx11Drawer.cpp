#include "Dx11Drawer.h"

#include "Dx11Instance.h"
#include "Dx11Viewer.h"
#include "../Assist/Hlsl/HlslConstants.h"
#include "../Assist/ViewerMatrix.h"
#include "../../Model/Model.h"

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
		? texture.textureView.Get() : info.viewer->GetDx11Info().dummyTexture.textureView.Get();
		ID3D11SamplerState* samplers = texture.texture
		? sampler : info.viewer->GetDx11Info().pipelineStates.textureSampler.Get();
		if (lastView != views) {
			info.viewer->GetDx11Info().deviceResources.context->PSSetShaderResources(slot, 1, &views);
			lastView = views;
		}
		if (lastSampler != samplers) {
			info.viewer->GetDx11Info().deviceResources.context->PSSetSamplers(slot, 1, &samplers);
			lastSampler = samplers;
		}
	}

	void Dx11Drawer::DrawModel() {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& vertexBuffer = info.vertexBuffer;
		const auto& indexBuffer = info.indexBuffer;
		const auto indexBufferFormat = info.indexBufferFormat;
		const auto& vsConstantBuffer = info.vsConstantBuffer;
		const auto& psConstantBuffer = info.psConstantBuffer;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto lightDir = glm::mat3(viewer->GetInfo().viewMat) * viewer->GetInfo().lightDir;
		const auto wv = view * world;
		const auto wvp = ViewerMatrix::DirectXClipMatrix() * proj * view * world;
		HlslModelPixelConstants basePsCb{};
		basePsCb.lightColor = viewer->GetInfo().lightColor;
		basePsCb.lightDir = lightDir;
		viewer->GetDx11Info().deviceResources.context->OMSetDepthStencilState(viewer->GetDx11Info().pipelineStates.defaultDss.Get(), 0x00);
		constexpr UINT stride = sizeof(Dx11Vertex);
		constexpr UINT offset = 0;
		viewer->GetDx11Info().deviceResources.context->IASetInputLayout(viewer->GetDx11Info().shaders.model.inputLayout.Get());
		viewer->GetDx11Info().deviceResources.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->GetDx11Info().deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->GetDx11Info().deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		HlslModelVertexConstants vsCb;
		vsCb.wv = wv;
		vsCb.wvp = wvp;
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetShader(viewer->GetDx11Info().shaders.model.vertexShader.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetShader(viewer->GetDx11Info().shaders.model.pixelShader.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		ID3D11ShaderResourceView* boundViews[3] = { nullptr, nullptr, nullptr };
		ID3D11SamplerState* boundSamplers[3] = { nullptr, nullptr, nullptr };
		const ID3D11RasterizerState* currentRs = nullptr;
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			HlslModelPixelConstants psCb = basePsCb;
			psCb.alpha         = mat.diffuse.a;
			psCb.diffuse       = mat.diffuse;
			psCb.ambient       = mat.ambient;
			psCb.specular      = mat.specular;
			psCb.specularPower = mat.specularPower;
			int baseMode = 0;
			if (material.texture.texture)
				baseMode = !material.texture.hasAlpha ? 1 : 2;
			BindTexture(
				0, material.texture, viewer->GetDx11Info().pipelineStates.textureSampler.Get(), baseMode,
				psCb.textureModes.x, psCb.texMulFactor, psCb.texAddFactor, mat.textureMulFactor, mat.textureAddFactor,
				boundViews[0], boundSamplers[0]
			);
			BindTexture(
				1, material.toonTexture, viewer->GetDx11Info().pipelineStates.toonTextureSampler.Get(), 1,
				psCb.textureModes.y, psCb.toonTexMulFactor, psCb.toonTexAddFactor, mat.toonTextureMulFactor, mat.toonTextureAddFactor,
				boundViews[1], boundSamplers[1]
			);
			int spMode = 0;
			if (material.spTexture.texture) {
				if (mat.spTextureMode == SphereMode::Mul)
					spMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					spMode = 2;
			}
			BindTexture(
				2, material.spTexture, viewer->GetDx11Info().pipelineStates.textureSampler.Get(), spMode,
				psCb.textureModes.z, psCb.sphereTexMulFactor, psCb.sphereTexAddFactor, mat.sphereTextureMulFactor, mat.sphereTextureAddFactor,
				boundViews[2], boundSamplers[2]
			);
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDx11Info().deviceResources.context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			ID3D11RasterizerState* targetRs = mat.bothFace
				? viewer->GetDx11Info().pipelineStates.bothFaceRs.Get()
				: viewer->GetDx11Info().pipelineStates.frontFaceRs.Get();
			if (currentRs != targetRs) {
				viewer->GetDx11Info().deviceResources.context->RSSetState(targetRs);
				currentRs = targetRs;
			}
			viewer->GetDx11Info().deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawEdge() {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& edgeVsConstantBuffer = info.edgeVsConstantBuffer;
		const auto& edgeSizeVsConstantBuffer = info.edgeSizeVsConstantBuffer;
		const auto& edgePsConstantBuffer = info.edgePsConstantBuffer;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto wv = view * world;
		const auto wvp = ViewerMatrix::DirectXClipMatrix() * proj * view * world;
		viewer->GetDx11Info().deviceResources.context->IASetInputLayout(viewer->GetDx11Info().shaders.edge.inputLayout.Get());
		HlslEdgeVertexConstants vsCb1{};
		vsCb1.wv = wv;
		vsCb1.wvp = wvp;
		vsCb1.screenSize = glm::vec2(viewer->GetInfo().screenWidth, viewer->GetInfo().screenHeight);
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(edgeVsConstantBuffer.Get(), 0, nullptr, &vsCb1, 0, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetShader(viewer->GetDx11Info().shaders.edge.vertexShader.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetShader(viewer->GetDx11Info().shaders.edge.pixelShader.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		viewer->GetDx11Info().deviceResources.context->RSSetState(viewer->GetDx11Info().pipelineStates.edgeRs.Get());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			HlslEdgeSizeConstants vsCb2{};
			vsCb2.edgeSize = mat.edgeSize;
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(edgeSizeVsConstantBuffer.Get(), 0, nullptr, &vsCb2, 0, 0);
			viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(1, 1, edgeSizeVsConstantBuffer.GetAddressOf());
			HlslEdgePixelConstants psCb{};
			psCb.edgeColor = mat.edgeColor;
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(edgePsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDx11Info().deviceResources.context->PSSetConstantBuffers(2, 1, edgePsConstantBuffer.GetAddressOf());
			viewer->GetDx11Info().deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawGroundShadow() {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& gsVsConstantBuffer = info.gsVsConstantBuffer;
		const auto& gsPsConstantBuffer = info.gsPsConstantBuffer;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		viewer->GetDx11Info().deviceResources.context->IASetInputLayout(viewer->GetDx11Info().shaders.groundShadow.inputLayout.Get());
		constexpr glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
		const glm::vec4 light(-viewer->GetInfo().lightDir, 0.f);
		const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.f) - glm::outerProduct(light, plane);
		HlslGroundShadowVertexConstants vsCb;
		vsCb.wvp = ViewerMatrix::DirectXClipMatrix() * proj * view * shadow * world;
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(gsVsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetShader(viewer->GetDx11Info().shaders.groundShadow.vertexShader.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetShader(viewer->GetDx11Info().shaders.groundShadow.pixelShader.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		viewer->GetDx11Info().deviceResources.context->RSSetState(viewer->GetDx11Info().pipelineStates.gsRs.Get());
		viewer->GetDx11Info().deviceResources.context->OMSetDepthStencilState(viewer->GetDx11Info().pipelineStates.gsDss.Get(), 0x01);
		HlslGroundShadowPixelConstants psCb;
		psCb.shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(gsPsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			viewer->GetDx11Info().deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	Dx11Drawer::Dx11Drawer(const Dx11InstanceInfo& sourceInfo) : info(sourceInfo) {}
}
