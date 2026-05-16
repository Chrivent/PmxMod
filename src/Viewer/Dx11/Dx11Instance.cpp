#include "Dx11Instance.h"

#include "Dx11Viewer.h"
#include "../../Model/ModelPose.h"

namespace Chrivent {
	void Dx11Instance::DrawModel() const {
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		const auto wv = view * world;
		const auto wvp = DxClipMatrix() * proj * view * world;
		viewer->context->OMSetDepthStencilState(viewer->defaultDss.Get(), 0x00);
		constexpr UINT stride = sizeof(Dx11Vertex);
		constexpr UINT offset = 0;
		viewer->context->IASetInputLayout(viewer->inputLayout.Get());
		viewer->context->IASetVertexBuffers(0, 1,
			vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Dx11VertexShader vsCb;
		vsCb.wv = wv;
		vsCb.wvp = wvp;
		viewer->context->UpdateSubresource(vsConstantBuffer.Get(),
			0, nullptr, &vsCb, 0, 0);
		viewer->context->VSSetShader(viewer->vs.Get(), nullptr, 0);
		viewer->context->PSSetShader(viewer->ps.Get(), nullptr, 0);
		viewer->context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : model->materialData.subMeshes) {
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
				0, material.texture, viewer->textureSampler.Get(), baseMode, psCb.textureModes.x,
				psCb.texMulFactor, psCb.texAddFactor, mat.textureMulFactor, mat.textureAddFactor);
			BindTexture(
				1, material.cartoonTexture, viewer->cartoonTextureSampler.Get(), 1, psCb.textureModes.y,
				psCb.cartoonTexMulFactor, psCb.cartoonTexAddFactor, mat.cartoonTextureMulFactor, mat.cartoonTextureAddFactor);
			int spMode = 0;
			if (material.spTexture.texture) {
				if (mat.spTextureMode == SphereMode::Mul)
					spMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					spMode = 2;
			}
			BindTexture(
				2, material.spTexture, viewer->textureSampler.Get(), spMode, psCb.textureModes.z,
				psCb.sphereTexMulFactor, psCb.sphereTexAddFactor, mat.sphereTextureMulFactor, mat.sphereTextureAddFactor);
			psCb.lightColor = viewer->lightColor;
			psCb.lightDir = glm::mat3(viewer->viewMat) * viewer->lightDir;
			viewer->context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			if (mat.bothFace)
				viewer->context->RSSetState(viewer->bothFaceRs.Get());
			else
				viewer->context->RSSetState(viewer->frontFaceRs.Get());
			viewer->context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Instance::DrawEdge() const {
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		const auto wv = view * world;
		const auto wvp = DxClipMatrix() * proj * view * world;
		viewer->context->IASetInputLayout(viewer->edgeInputLayout.Get());
		Dx11EdgeVertexShader vsCb1{};
		vsCb1.wv = wv;
		vsCb1.wvp = wvp;
		vsCb1.screenSize = glm::vec2(
			static_cast<float>(viewer->screenWidth),
			static_cast<float>(viewer->screenHeight));
		viewer->context->UpdateSubresource(edgeVsConstantBuffer.Get(),
			0, nullptr, &vsCb1, 0, 0);
		viewer->context->VSSetShader(viewer->edgeVs.Get(), nullptr, 0);
		viewer->context->PSSetShader(viewer->edgePs.Get(), nullptr, 0);
		viewer->context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			Dx11EdgeSizeVertexShader vsCb2{};
			vsCb2.edgeSize = mat.edgeSize;
			viewer->context->UpdateSubresource(edgeSizeVsConstantBuffer.Get(),
				0, nullptr, &vsCb2, 0, 0);
			viewer->context->VSSetConstantBuffers(1, 1, edgeSizeVsConstantBuffer.GetAddressOf());
			Dx11EdgePixelShader psCb{};
			psCb.edgeColor = mat.edgeColor;
			viewer->context->UpdateSubresource(edgePsConstantBuffer.Get(),
				0, nullptr, &psCb, 0, 0);
			viewer->context->PSSetConstantBuffers(2, 1, edgePsConstantBuffer.GetAddressOf());
			viewer->context->RSSetState(viewer->edgeRs.Get());
			viewer->context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Instance::DrawGroundShadow() const {
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		viewer->context->IASetInputLayout(viewer->gsInputLayout.Get());
		constexpr glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
		const glm::vec4 light(-glm::normalize(viewer->lightDir), 0.f);
		const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.f) - glm::outerProduct(light, plane);
		Dx11GroundShadowVertexShader vsCb;
		vsCb.wvp = DxClipMatrix() * proj * view * shadow * world;
		viewer->context->UpdateSubresource(gsVsConstantBuffer.Get(),
			0, nullptr, &vsCb, 0, 0);
		viewer->context->VSSetShader(viewer->gsVs.Get(), nullptr, 0);
		viewer->context->PSSetShader(viewer->gsPs.Get(), nullptr, 0);
		viewer->context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		viewer->context->RSSetState(viewer->gsRs.Get());
		viewer->context->OMSetDepthStencilState(viewer->gsDss.Get(), 0x01);
		for (const auto& [beginIndex, indexCount, materialId] : model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			Dx11GroundShadowPixelShader psCb{};
			psCb.shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
			viewer->context->UpdateSubresource(gsPsConstantBuffer.Get(),
				0, nullptr, &psCb, 0, 0);
			viewer->context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
			viewer->context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	D3D11_BUFFER_DESC Dx11Instance::MakeVertexBufferDesc(const size_t vertexCount) {
		D3D11_BUFFER_DESC d{};
		d.Usage = D3D11_USAGE_DYNAMIC;
		d.ByteWidth = static_cast<UINT>(sizeof(Dx11Vertex) * vertexCount);
		d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		return d;
	}

	D3D11_BUFFER_DESC Dx11Instance::MakeIndexBufferDesc(const size_t indexBytes) {
		D3D11_BUFFER_DESC d{};
		d.Usage = D3D11_USAGE_IMMUTABLE;
		d.ByteWidth = static_cast<UINT>(indexBytes);
		d.BindFlags = D3D11_BIND_INDEX_BUFFER;
		return d;
	}

	void Dx11Instance::BindTexture(
		const UINT slot, const Dx11Texture& tex, ID3D11SamplerState* sampler, const int modeIfPresent,
		int& outMode, glm::vec4& outMul, glm::vec4& outAdd,
		const glm::vec4& mulIn, const glm::vec4& addIn) const {
		if (tex.texture) {
			outMode = modeIfPresent;
			outMul  = mulIn;
			outAdd  = addIn;
		} else
			outMode = 0;
		ID3D11ShaderResourceView* views = tex.texture ? tex.textureView.Get() : viewer->dummyTextureView.Get();
		ID3D11SamplerState* samplers = tex.texture ? sampler : viewer->textureSampler.Get();
		viewer->context->PSSetShaderResources(slot, 1, &views);
		viewer->context->PSSetSamplers(slot, 1, &samplers);
	}

	const glm::mat4& Dx11Instance::DxClipMatrix() {
		static constexpr glm::mat4 dxMat(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return dxMat;
	}

	bool Dx11Instance::Setup(Viewer& baseViewer) {
		viewer = &dynamic_cast<Dx11Viewer&>(baseViewer);
		const auto vBufDesc = MakeVertexBufferDesc(model->geometry.positions.size());
		if (FAILED(viewer->device->CreateBuffer(&vBufDesc, nullptr, &vertexBuffer)))
			return false;
		const auto iBufDesc = MakeIndexBufferDesc(model->geometry.indexElementSize * model->geometry.indexCount);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = model->geometry.indices.data();
		if (FAILED(viewer->device->CreateBuffer(&iBufDesc, &initData, &indexBuffer)))
			return false;
		if (1 == model->geometry.indexElementSize)
			indexBufferFormat = DXGI_FORMAT_R8_UINT;
		else if (2 == model->geometry.indexElementSize)
			indexBufferFormat = DXGI_FORMAT_R16_UINT;
		else if (4 == model->geometry.indexElementSize)
			indexBufferFormat = DXGI_FORMAT_R32_UINT;
		else
			return false;
		if (FAILED(CreateBuffer<Dx11VertexShader>(viewer->device.Get(), vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11PixelShader>(viewer->device.Get(), psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgeVertexShader>(viewer->device.Get(), edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgeSizeVertexShader>(viewer->device.Get(), edgeSizeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgePixelShader>(viewer->device.Get(), edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11GroundShadowVertexShader>(viewer->device.Get(), gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11GroundShadowPixelShader>(viewer->device.Get(), gsPsConstantBuffer)))
			return false;
		for (const auto& mat : model->materialData.materials) {
			Dx11Material material(mat);
			if (!mat.texture.empty())
				material.texture = viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.spTexture = viewer->LoadTexture(mat.spTexture);
			if (!mat.cartoonTexture.empty())
				material.cartoonTexture = viewer->LoadTexture(mat.cartoonTexture);
			materials.emplace_back(std::move(material));
		}
		return true;
	}

	void Dx11Instance::Update() const {
		const ModelPose pose(*model);
		pose.Update();
		const size_t vtxCount = model->geometry.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		if (FAILED(viewer->context->Map(vertexBuffer.Get(), 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
			return;
		const auto vertices = static_cast<Dx11Vertex*>(mapRes.pData);
		const glm::vec3* updatePositionData = model->geometry.updatePositions.data();
		const glm::vec3* updateNormalData = model->geometry.updateNormals.data();
		const glm::vec2* updateUvData = model->geometry.updateUVs.data();
		for (size_t i = 0; i < vtxCount; i++) {
			vertices[i].position = updatePositionData[i];
			vertices[i].normal = updateNormalData[i];
			vertices[i].uv = updateUvData[i];
		}
		viewer->context->Unmap(vertexBuffer.Get(), 0);
	}
}
