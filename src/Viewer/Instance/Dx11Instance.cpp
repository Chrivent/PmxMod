#include "Viewer/Instance/Dx11Instance.h"

#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/Viewer/Dx11Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

#include <limits>

namespace Chrivent {
	bool Dx11Instance::CreateGeometryBuffers() {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (geometryData.positions.empty() ||
			!ViewerGeometry::BuildIndexData(geometryData, indexData) ||
			indexData.bytes.empty())
			return false;
		const size_t vertexByteSize = sizeof(ViewerVertex) * geometryData.positions.size();
		if (vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max())
			return false;
		const auto vBufDesc = Dx11DescBuilder::MakeDynamicVertexBufferDesc(static_cast<UINT>(vertexByteSize));
		if (FAILED(viewer.GetDeviceResources().device->CreateBuffer(&vBufDesc, nullptr, &vertexBuffer)))
			return false;
		const auto iBufDesc = Dx11DescBuilder::MakeImmutableIndexBufferDesc(static_cast<UINT>(indexData.bytes.size()));
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = indexData.bytes.data();
		if (FAILED(viewer.GetDeviceResources().device->CreateBuffer(&iBufDesc, &initData, &indexBuffer)))
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
		if (FAILED(CreateBuffer<ModelVertexConstants>(viewer.GetDeviceResources().device.Get(), vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<ModelPixelConstants>(viewer.GetDeviceResources().device.Get(), psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<SceneSurfacePixelConstants>(
			viewer.GetDeviceResources().device.Get(), sceneSurfaceConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgeVertexConstants>(viewer.GetDeviceResources().device.Get(), edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgePixelConstants>(viewer.GetDeviceResources().device.Get(), edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowVertexConstants>(viewer.GetDeviceResources().device.Get(), gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowPixelConstants>(viewer.GetDeviceResources().device.Get(), gsPsConstantBuffer)))
			return false;
		return true;
	}

	void Dx11Instance::LoadMaterials() {
		for (const auto& mat : model->materialData.materials) {
			Dx11Material material(mat);
			if (!mat.texture.empty())
				material.texture = viewer.LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = viewer.LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = viewer.LoadTexture(mat.toonTexture);
			materials.emplace_back(std::move(material));
		}
	}

	Dx11Instance::Dx11Instance(Dx11Viewer& sourceViewer) : viewer(sourceViewer) {
		drawer = std::make_unique<Dx11Drawer>(*this, viewer);
	}

	void Dx11Instance::ResetRendererResources() {
		materials.clear();
		vertexBuffer.Reset();
		indexBuffer.Reset();
		vsConstantBuffer.Reset();
		psConstantBuffer.Reset();
		sceneSurfaceConstantBuffer.Reset();
		edgeVsConstantBuffer.Reset();
		edgePsConstantBuffer.Reset();
		gsVsConstantBuffer.Reset();
		gsPsConstantBuffer.Reset();
		indexBufferFormat = DXGI_FORMAT_R16_UINT;
	}

	bool Dx11Instance::SetupRenderer() {
		if (!CreateGeometryBuffers())
			return false;
		if (!CreateConstantBuffers())
			return false;
		LoadMaterials();
		return true;
	}

	bool Dx11Instance::Upload() const {
		const size_t vtxCount = model->geometryData.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		if (FAILED(viewer.GetDeviceResources().context->Map(vertexBuffer.Get(), 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
			return false;
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(mapRes.pData), vtxCount });
		viewer.GetDeviceResources().context->Unmap(vertexBuffer.Get(), 0);
		return writeSucceeded;
	}
}

