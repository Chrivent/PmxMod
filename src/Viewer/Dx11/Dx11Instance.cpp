#include "Dx11Instance.h"

#include "Dx11Drawer.h"

#include "Dx11Viewer.h"
#include "../ShaderConstants.h"
#include "Helper/Dx11DescBuilder.h"
#include "../ViewerGeometry.h"
#include "../../Core/Model/Model.h"

#include <limits>

namespace Chrivent {
	bool Dx11Instance::CreateGeometryBuffers() {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (geometryData.positions.empty() ||
			!ViewerGeometry::BuildIndexData(geometryData, indexData) ||
			indexData.bytes.empty())
			return false;
		const size_t vertexByteSize = sizeof(Dx11Vertex) * geometryData.positions.size();
		if (vertexByteSize > (std::numeric_limits<UINT>::max)() ||
			indexData.bytes.size() > (std::numeric_limits<UINT>::max)())
			return false;
		const auto vBufDesc = Dx11DescBuilder::MakeDynamicVertexBufferDesc(static_cast<UINT>(vertexByteSize));
		if (FAILED(viewer->deviceResources.device->CreateBuffer(&vBufDesc, nullptr, &vertexBuffer)))
			return false;
		const auto iBufDesc = Dx11DescBuilder::MakeImmutableIndexBufferDesc(static_cast<UINT>(indexData.bytes.size()));
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = indexData.bytes.data();
		if (FAILED(viewer->deviceResources.device->CreateBuffer(&iBufDesc, &initData, &indexBuffer)))
			return false;
		if (indexData.elementSize == sizeof(uint16_t))
			indexBufferFormat = DXGI_FORMAT_R16_UINT;
		else if (indexData.elementSize == sizeof(uint32_t))
			indexBufferFormat = DXGI_FORMAT_R32_UINT;
		else
			return false;
		return true;
	}

	bool Dx11Instance::CreateConstantBuffers() {
		if (FAILED(CreateBuffer<ModelVertexConstants>(viewer->deviceResources.device.Get(), vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<ModelPixelConstants>(viewer->deviceResources.device.Get(), psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgeVertexConstants>(viewer->deviceResources.device.Get(), edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgePixelConstants>(viewer->deviceResources.device.Get(), edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowVertexConstants>(viewer->deviceResources.device.Get(), gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowPixelConstants>(viewer->deviceResources.device.Get(), gsPsConstantBuffer)))
			return false;
		return true;
	}

	void Dx11Instance::LoadMaterials() {
		for (const auto& mat : model->materialData.materials) {
			Dx11Material material(mat);
			if (!mat.texture.empty())
				material.texture = viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.spTexture = viewer->LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = viewer->LoadTexture(mat.toonTexture);
			materials.emplace_back(std::move(material));
		}
	}

	bool Dx11Instance::Setup(Viewer& baseViewer) {
		viewer = &static_cast<Dx11Viewer&>(baseViewer);
		drawer = std::make_unique<Dx11Drawer>(*this);
		if (!CreateGeometryBuffers())
			return false;
		if (!CreateConstantBuffers())
			return false;
		LoadMaterials();
		return true;
	}

	void Dx11Instance::Upload() const {
		const size_t vtxCount = model->geometryData.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		if (FAILED(viewer->deviceResources.context->Map(vertexBuffer.Get(), 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
			return;
		ViewerGeometry::WriteVertices(model->geometryData, true, static_cast<Dx11Vertex*>(mapRes.pData), vtxCount);
		viewer->deviceResources.context->Unmap(vertexBuffer.Get(), 0);
	}
}

