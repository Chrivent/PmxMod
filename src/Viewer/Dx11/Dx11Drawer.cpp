#include "Dx11Drawer.h"

#include "Dx11Instance.h"
#include "Dx11Viewer.h"
#include "../../Model/Model.h"

namespace Chrivent {
	Dx11Drawer::Dx11Drawer(const Dx11InstanceInfo& sourceInfo) : info(sourceInfo) {}

	void Dx11Drawer::DrawModel() const {
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
		const auto wv = view * world;
		const auto wvp = DxClipMatrix() * proj * view * world;
		viewer->GetDx11Info().deviceResources.context->OMSetDepthStencilState(viewer->GetDx11Info().pipelineStates.defaultDss.Get(), 0x00);
		constexpr UINT stride = sizeof(Dx11Vertex);
		constexpr UINT offset = 0;
		viewer->GetDx11Info().deviceResources.context->IASetInputLayout(viewer->GetDx11Info().shaders.inputLayout.Get());
		viewer->GetDx11Info().deviceResources.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->GetDx11Info().deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->GetDx11Info().deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Dx11VertexShader vsCb;
		vsCb.wv = wv;
		vsCb.wvp = wvp;
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(vsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetShader(viewer->GetDx11Info().shaders.vs.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetShader(viewer->GetDx11Info().shaders.ps.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			Dx11PixelShader psCb{};
			psCb.alpha         = mat.diffuse.a;
			psCb.diffuse       = mat.diffuse;
			psCb.ambient       = mat.ambient;
			psCb.specular      = mat.specular;
			psCb.specularPower = mat.specularPower;
			int baseMode = 0;
			if (material.texture.texture)
				baseMode = !material.texture.hasAlpha ? 1 : 2;
			BindTexture(
				0, material.texture, viewer->GetDx11Info().pipelineStates.textureSampler.Get(), baseMode, psCb.textureModes.x,
				psCb.texMulFactor, psCb.texAddFactor, mat.textureMulFactor, mat.textureAddFactor);
			BindTexture(
				1, material.cartoonTexture, viewer->GetDx11Info().pipelineStates.cartoonTextureSampler.Get(), 1, psCb.textureModes.y,
				psCb.cartoonTexMulFactor, psCb.cartoonTexAddFactor, mat.cartoonTextureMulFactor, mat.cartoonTextureAddFactor);
			int spMode = 0;
			if (material.spTexture.texture) {
				if (mat.spTextureMode == SphereMode::Mul)
					spMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					spMode = 2;
			}
			BindTexture(
				2, material.spTexture, viewer->GetDx11Info().pipelineStates.textureSampler.Get(), spMode, psCb.textureModes.z,
				psCb.sphereTexMulFactor, psCb.sphereTexAddFactor, mat.sphereTextureMulFactor, mat.sphereTextureAddFactor);
			psCb.lightColor = viewer->GetInfo().lightColor;
			psCb.lightDir = glm::mat3(viewer->GetInfo().viewMat) * viewer->GetInfo().lightDir;
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDx11Info().deviceResources.context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			if (mat.bothFace)
				viewer->GetDx11Info().deviceResources.context->RSSetState(viewer->GetDx11Info().pipelineStates.bothFaceRs.Get());
			else
				viewer->GetDx11Info().deviceResources.context->RSSetState(viewer->GetDx11Info().pipelineStates.frontFaceRs.Get());
			viewer->GetDx11Info().deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawEdge() const {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& edgeVsConstantBuffer = info.edgeVsConstantBuffer;
		const auto& edgeSizeVsConstantBuffer = info.edgeSizeVsConstantBuffer;
		const auto& edgePsConstantBuffer = info.edgePsConstantBuffer;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto wv = view * world;
		const auto wvp = DxClipMatrix() * proj * view * world;
		viewer->GetDx11Info().deviceResources.context->IASetInputLayout(viewer->GetDx11Info().shaders.edgeInputLayout.Get());
		Dx11EdgeVertexShader vsCb1{};
		vsCb1.wv = wv;
		vsCb1.wvp = wvp;
		vsCb1.screenSize = glm::vec2(static_cast<float>(viewer->GetInfo().screenWidth), static_cast<float>(viewer->GetInfo().screenHeight));
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(edgeVsConstantBuffer.Get(), 0, nullptr, &vsCb1, 0, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetShader(viewer->GetDx11Info().shaders.edgeVs.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetShader(viewer->GetDx11Info().shaders.edgePs.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			Dx11EdgeSizeVertexShader vsCb2{};
			vsCb2.edgeSize = mat.edgeSize;
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(edgeSizeVsConstantBuffer.Get(), 0, nullptr, &vsCb2, 0, 0);
			viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(1, 1, edgeSizeVsConstantBuffer.GetAddressOf());
			Dx11EdgePixelShader psCb{};
			psCb.edgeColor = mat.edgeColor;
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(edgePsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDx11Info().deviceResources.context->PSSetConstantBuffers(2, 1, edgePsConstantBuffer.GetAddressOf());
			viewer->GetDx11Info().deviceResources.context->RSSetState(viewer->GetDx11Info().pipelineStates.edgeRs.Get());
			viewer->GetDx11Info().deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawGroundShadow() const {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& gsVsConstantBuffer = info.gsVsConstantBuffer;
		const auto& gsPsConstantBuffer = info.gsPsConstantBuffer;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		viewer->GetDx11Info().deviceResources.context->IASetInputLayout(viewer->GetDx11Info().shaders.gsInputLayout.Get());
		constexpr glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
		const glm::vec4 light(-glm::normalize(viewer->GetInfo().lightDir), 0.f);
		const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.f) - glm::outerProduct(light, plane);
		Dx11GroundShadowVertexShader vsCb;
		vsCb.wvp = DxClipMatrix() * proj * view * shadow * world;
		viewer->GetDx11Info().deviceResources.context->UpdateSubresource(gsVsConstantBuffer.Get(), 0, nullptr, &vsCb, 0, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetShader(viewer->GetDx11Info().shaders.gsVs.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->PSSetShader(viewer->GetDx11Info().shaders.gsPs.Get(), nullptr, 0);
		viewer->GetDx11Info().deviceResources.context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		viewer->GetDx11Info().deviceResources.context->RSSetState(viewer->GetDx11Info().pipelineStates.gsRs.Get());
		viewer->GetDx11Info().deviceResources.context->OMSetDepthStencilState(viewer->GetDx11Info().pipelineStates.gsDss.Get(), 0x01);
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			Dx11GroundShadowPixelShader psCb{};
			psCb.shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
			viewer->GetDx11Info().deviceResources.context->UpdateSubresource(gsPsConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->GetDx11Info().deviceResources.context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
			viewer->GetDx11Info().deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::BindTexture(
		const UINT slot, const Dx11Texture& tex, ID3D11SamplerState* sampler, const int modeIfPresent,
		int& outMode, glm::vec4& outMul, glm::vec4& outAdd,
		const glm::vec4& mulIn, const glm::vec4& addIn) const {
		if (tex.texture) {
			outMode = modeIfPresent;
			outMul  = mulIn;
			outAdd  = addIn;
		} else
			outMode = 0;
		ID3D11ShaderResourceView* views = tex.texture ? tex.textureView.Get() : info.viewer->GetDx11Info().dummyTexture.textureView.Get();
		ID3D11SamplerState* samplers = tex.texture ? sampler : info.viewer->GetDx11Info().pipelineStates.textureSampler.Get();
		info.viewer->GetDx11Info().deviceResources.context->PSSetShaderResources(slot, 1, &views);
		info.viewer->GetDx11Info().deviceResources.context->PSSetSamplers(slot, 1, &samplers);
	}

	const glm::mat4& Dx11Drawer::DxClipMatrix() {
		static constexpr glm::mat4 dxMat(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return dxMat;
	}
}
