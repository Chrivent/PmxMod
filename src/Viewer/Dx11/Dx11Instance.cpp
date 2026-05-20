#include "Dx11Instance.h"

#include "Dx11Drawer.h"

#include "Dx11Viewer.h"
#include "../../Model/ModelPose.h"

namespace Chrivent {
	Dx11Drawer::Dx11Drawer(const Dx11Instance& sourceInstance) : instance(sourceInstance) {}

	void Dx11Drawer::DrawModel() const {
		const auto& info = instance.GetDx11Info();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& vertexBuffer = info.vertexBuffer;
		const auto& indexBuffer = info.indexBuffer;
		const auto indexBufferFormat = info.indexBufferFormat;
		const auto& vsConstantBuffer = info.vsConstantBuffer;
		const auto& psConstantBuffer = info.psConstantBuffer;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto wv = view * world;
		const auto wvp = Dx11Instance::DxClipMatrix() * proj * view * world;
		viewer->deviceResources.context->OMSetDepthStencilState(viewer->pipelineStates.defaultDss.Get(), 0x00);
		constexpr UINT stride = sizeof(Dx11Vertex);
		constexpr UINT offset = 0;
		viewer->deviceResources.context->IASetInputLayout(viewer->shaders.inputLayout.Get());
		viewer->deviceResources.context->IASetVertexBuffers(0, 1,
			vertexBuffer.GetAddressOf(), &stride, &offset);
		viewer->deviceResources.context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
		viewer->deviceResources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Dx11VertexShader vsCb;
		vsCb.wv = wv;
		vsCb.wvp = wvp;
		viewer->deviceResources.context->UpdateSubresource(vsConstantBuffer.Get(),
			0, nullptr, &vsCb, 0, 0);
		viewer->deviceResources.context->VSSetShader(viewer->shaders.vs.Get(), nullptr, 0);
		viewer->deviceResources.context->PSSetShader(viewer->shaders.ps.Get(), nullptr, 0);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
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
			instance.BindTexture(
				0, material.texture, viewer->pipelineStates.textureSampler.Get(), baseMode, psCb.textureModes.x,
				psCb.texMulFactor, psCb.texAddFactor, mat.textureMulFactor, mat.textureAddFactor);
			instance.BindTexture(
				1, material.cartoonTexture, viewer->pipelineStates.cartoonTextureSampler.Get(), 1, psCb.textureModes.y,
				psCb.cartoonTexMulFactor, psCb.cartoonTexAddFactor, mat.cartoonTextureMulFactor, mat.cartoonTextureAddFactor);
			int spMode = 0;
			if (material.spTexture.texture) {
				if (mat.spTextureMode == SphereMode::Mul)
					spMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					spMode = 2;
			}
			instance.BindTexture(
				2, material.spTexture, viewer->pipelineStates.textureSampler.Get(), spMode, psCb.textureModes.z,
				psCb.sphereTexMulFactor, psCb.sphereTexAddFactor, mat.sphereTextureMulFactor, mat.sphereTextureAddFactor);
			psCb.lightColor = viewer->lightColor;
			psCb.lightDir = glm::mat3(viewer->viewMat) * viewer->lightDir;
			viewer->deviceResources.context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCb, 0, 0);
			viewer->deviceResources.context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
			if (mat.bothFace)
				viewer->deviceResources.context->RSSetState(viewer->pipelineStates.bothFaceRs.Get());
			else
				viewer->deviceResources.context->RSSetState(viewer->pipelineStates.frontFaceRs.Get());
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawEdge() const {
		const auto& info = instance.GetDx11Info();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& edgeVsConstantBuffer = info.edgeVsConstantBuffer;
		const auto& edgeSizeVsConstantBuffer = info.edgeSizeVsConstantBuffer;
		const auto& edgePsConstantBuffer = info.edgePsConstantBuffer;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto wv = view * world;
		const auto wvp = Dx11Instance::DxClipMatrix() * proj * view * world;
		viewer->deviceResources.context->IASetInputLayout(viewer->shaders.edgeInputLayout.Get());
		Dx11EdgeVertexShader vsCb1{};
		vsCb1.wv = wv;
		vsCb1.wvp = wvp;
		vsCb1.screenSize = glm::vec2(
			static_cast<float>(viewer->screenWidth),
			static_cast<float>(viewer->screenHeight));
		viewer->deviceResources.context->UpdateSubresource(edgeVsConstantBuffer.Get(),
			0, nullptr, &vsCb1, 0, 0);
		viewer->deviceResources.context->VSSetShader(viewer->shaders.edgeVs.Get(), nullptr, 0);
		viewer->deviceResources.context->PSSetShader(viewer->shaders.edgePs.Get(), nullptr, 0);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			Dx11EdgeSizeVertexShader vsCb2{};
			vsCb2.edgeSize = mat.edgeSize;
			viewer->deviceResources.context->UpdateSubresource(edgeSizeVsConstantBuffer.Get(),
				0, nullptr, &vsCb2, 0, 0);
			viewer->deviceResources.context->VSSetConstantBuffers(1, 1, edgeSizeVsConstantBuffer.GetAddressOf());
			Dx11EdgePixelShader psCb{};
			psCb.edgeColor = mat.edgeColor;
			viewer->deviceResources.context->UpdateSubresource(edgePsConstantBuffer.Get(),
				0, nullptr, &psCb, 0, 0);
			viewer->deviceResources.context->PSSetConstantBuffers(2, 1, edgePsConstantBuffer.GetAddressOf());
			viewer->deviceResources.context->RSSetState(viewer->pipelineStates.edgeRs.Get());
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
		}
	}

	void Dx11Drawer::DrawGroundShadow() const {
		const auto& info = instance.GetDx11Info();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto& gsVsConstantBuffer = info.gsVsConstantBuffer;
		const auto& gsPsConstantBuffer = info.gsPsConstantBuffer;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		viewer->deviceResources.context->IASetInputLayout(viewer->shaders.gsInputLayout.Get());
		constexpr glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
		const glm::vec4 light(-glm::normalize(viewer->lightDir), 0.f);
		const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.f) - glm::outerProduct(light, plane);
		Dx11GroundShadowVertexShader vsCb;
		vsCb.wvp = Dx11Instance::DxClipMatrix() * proj * view * shadow * world;
		viewer->deviceResources.context->UpdateSubresource(gsVsConstantBuffer.Get(),
			0, nullptr, &vsCb, 0, 0);
		viewer->deviceResources.context->VSSetShader(viewer->shaders.gsVs.Get(), nullptr, 0);
		viewer->deviceResources.context->PSSetShader(viewer->shaders.gsPs.Get(), nullptr, 0);
		viewer->deviceResources.context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
		viewer->deviceResources.context->RSSetState(viewer->pipelineStates.gsRs.Get());
		viewer->deviceResources.context->OMSetDepthStencilState(viewer->pipelineStates.gsDss.Get(), 0x01);
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0)
				continue;
			Dx11GroundShadowPixelShader psCb{};
			psCb.shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
			viewer->deviceResources.context->UpdateSubresource(gsPsConstantBuffer.Get(),
				0, nullptr, &psCb, 0, 0);
			viewer->deviceResources.context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
			viewer->deviceResources.context->DrawIndexed(indexCount, beginIndex, 0);
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
		const auto& info = GetDx11Info();
		ID3D11ShaderResourceView* views = tex.texture ? tex.textureView.Get() : info.viewer->dummyTexture.textureView.Get();
		ID3D11SamplerState* samplers = tex.texture ? sampler : info.viewer->pipelineStates.textureSampler.Get();
		info.viewer->deviceResources.context->PSSetShaderResources(slot, 1, &views);
		info.viewer->deviceResources.context->PSSetSamplers(slot, 1, &samplers);
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

	Dx11Instance::Dx11Instance() {
		info = std::make_unique<Dx11InstanceInfo>();
	}

	bool Dx11Instance::Setup(Viewer& baseViewer) {
		auto& info = GetDx11Info();
		info.viewer = &dynamic_cast<Dx11Viewer&>(baseViewer);
		drawer = std::make_unique<Dx11Drawer>(*this);
		const auto vBufDesc = MakeVertexBufferDesc(info.model->geometryData.positions.size());
		if (FAILED(info.viewer->deviceResources.device->CreateBuffer(&vBufDesc, nullptr, &info.vertexBuffer)))
			return false;
		const auto iBufDesc = MakeIndexBufferDesc(info.model->geometryData.indexElementSize * info.model->geometryData.indexCount);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = info.model->geometryData.indices.data();
		if (FAILED(info.viewer->deviceResources.device->CreateBuffer(&iBufDesc, &initData, &info.indexBuffer)))
			return false;
		if (1 == info.model->geometryData.indexElementSize)
			info.indexBufferFormat = DXGI_FORMAT_R8_UINT;
		else if (2 == info.model->geometryData.indexElementSize)
			info.indexBufferFormat = DXGI_FORMAT_R16_UINT;
		else if (4 == info.model->geometryData.indexElementSize)
			info.indexBufferFormat = DXGI_FORMAT_R32_UINT;
		else
			return false;
		if (FAILED(CreateBuffer<Dx11VertexShader>(info.viewer->deviceResources.device.Get(), info.vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11PixelShader>(info.viewer->deviceResources.device.Get(), info.psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgeVertexShader>(info.viewer->deviceResources.device.Get(), info.edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgeSizeVertexShader>(info.viewer->deviceResources.device.Get(), info.edgeSizeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgePixelShader>(info.viewer->deviceResources.device.Get(), info.edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11GroundShadowVertexShader>(info.viewer->deviceResources.device.Get(), info.gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11GroundShadowPixelShader>(info.viewer->deviceResources.device.Get(), info.gsPsConstantBuffer)))
			return false;
		for (const auto& mat : info.model->materialData.materials) {
			Dx11Material material(mat);
			if (!mat.texture.empty())
				material.texture = info.viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.spTexture = info.viewer->LoadTexture(mat.spTexture);
			if (!mat.cartoonTexture.empty())
				material.cartoonTexture = info.viewer->LoadTexture(mat.cartoonTexture);
			info.materials.emplace_back(std::move(material));
		}
		return true;
	}

	void Dx11Instance::Update() const {
		const auto& info = GetDx11Info();
		const ModelPose pose(*info.model);
		pose.Update();
		const size_t vtxCount = info.model->geometryData.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		if (FAILED(info.viewer->deviceResources.context->Map(info.vertexBuffer.Get(), 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
			return;
		const auto vertices = static_cast<Dx11Vertex*>(mapRes.pData);
		const glm::vec3* updatePositionData = info.model->geometryData.updatePositions.data();
		const glm::vec3* updateNormalData = info.model->geometryData.updateNormals.data();
		const glm::vec2* updateUvData = info.model->geometryData.updateUVs.data();
		for (size_t i = 0; i < vtxCount; i++) {
			vertices[i].position = updatePositionData[i];
			vertices[i].normal = updateNormalData[i];
			vertices[i].uv = updateUvData[i];
		}
		info.viewer->deviceResources.context->Unmap(info.vertexBuffer.Get(), 0);
	}
}


