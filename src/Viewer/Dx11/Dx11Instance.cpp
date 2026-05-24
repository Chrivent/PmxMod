#include "Dx11Instance.h"

#include "Dx11Drawer.h"

#include "Dx11Viewer.h"
#include "Helper/Dx11DescriptorFactory.h"
#include "../../Model/ModelPose.h"

namespace Chrivent {
	Dx11Instance::Dx11Instance() {
		info = std::make_unique<Dx11InstanceInfo>();
	}

	bool Dx11Instance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<Dx11InstanceInfo&>(GetInfo());;
		info.viewer = &dynamic_cast<Dx11Viewer&>(baseViewer);
		drawer = std::make_unique<Dx11Drawer>(info);
		const auto vBufDesc = Dx11DescriptorFactory::MakeDynamicVertexBufferDesc(
			sizeof(Dx11Vertex) * info.model->geometryData.positions.size());
		if (FAILED(info.viewer->GetDx11Info().deviceResources.device->CreateBuffer(&vBufDesc, nullptr, &info.vertexBuffer)))
			return false;
		const auto iBufDesc = Dx11DescriptorFactory::MakeImmutableIndexBufferDesc(
			info.model->geometryData.indexElementSize * info.model->geometryData.indexCount);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = info.model->geometryData.indices.data();
		if (FAILED(info.viewer->GetDx11Info().deviceResources.device->CreateBuffer(&iBufDesc, &initData, &info.indexBuffer)))
			return false;
		if (1 == info.model->geometryData.indexElementSize)
			info.indexBufferFormat = DXGI_FORMAT_R8_UINT;
		else if (2 == info.model->geometryData.indexElementSize)
			info.indexBufferFormat = DXGI_FORMAT_R16_UINT;
		else if (4 == info.model->geometryData.indexElementSize)
			info.indexBufferFormat = DXGI_FORMAT_R32_UINT;
		else
			return false;
		if (FAILED(CreateBuffer<Dx11ModelVertexConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11ModelPixelConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgeVertexConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgeSizeConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.edgeSizeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11EdgePixelConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11GroundShadowVertexConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<Dx11GroundShadowPixelConstants>(info.viewer->GetDx11Info().deviceResources.device.Get(), info.gsPsConstantBuffer)))
			return false;
		for (const auto& mat : info.model->materialData.materials) {
			Dx11Material material(mat);
			if (!mat.texture.empty())
				material.texture = info.viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.spTexture = info.viewer->LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = info.viewer->LoadTexture(mat.toonTexture);
			info.materials.emplace_back(std::move(material));
		}
		return true;
	}

	void Dx11Instance::Update() const {
		const auto& info = static_cast<const Dx11InstanceInfo&>(GetInfo());
		const ModelPose pose(*info.model);
		pose.Update();
		const size_t vtxCount = info.model->geometryData.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		if (FAILED(info.viewer->GetDx11Info().deviceResources.context->Map(info.vertexBuffer.Get(), 0,
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
		info.viewer->GetDx11Info().deviceResources.context->Unmap(info.vertexBuffer.Get(), 0);
	}
}

