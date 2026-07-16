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
		ID3D11Device* device = viewer.GetDrawContext().GetDevice();
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData))
			return false;
		const size_t vertexByteSize = sizeof(ViewerVertex) * geometryData.positions.size();
		if (vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max())
			return false;
		const auto vBufDesc = Dx11DescBuilder::MakeDynamicVertexBufferDesc(static_cast<UINT>(vertexByteSize));
		if (FAILED(device->CreateBuffer(
			&vBufDesc, nullptr, &modelResources.vertexBuffer)))
			return false;
		const auto iBufDesc = Dx11DescBuilder::MakeImmutableIndexBufferDesc(static_cast<UINT>(indexData.bytes.size()));
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = indexData.bytes.data();
		if (FAILED(device->CreateBuffer(
			&iBufDesc, &initData, &modelResources.indexBuffer)))
			return false;
		if (indexData.elementSize == sizeof(uint16_t))
			modelResources.indexBufferFormat = DXGI_FORMAT_R16_UINT;
		else if (indexData.elementSize == sizeof(uint32_t))
			modelResources.indexBufferFormat = DXGI_FORMAT_R32_UINT;
		else
			return false;
		return true;
	}

	bool Dx11Instance::CreateConstantBuffers() {
		ID3D11Device* device = viewer.GetDrawContext().GetDevice();
		if (FAILED(CreateBuffer<ModelVertexConstants>(
			device, modelResources.vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<ModelPixelConstants>(
			device, modelResources.psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<SceneSurfacePixelConstants>(
			device, modelResources.sceneSurfaceConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgeVertexConstants>(
			device, modelResources.edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgePixelConstants>(
			device, modelResources.edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowVertexConstants>(
			device, modelResources.gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowPixelConstants>(
			device, modelResources.gsPsConstantBuffer)))
			return false;
		return true;
	}

	void Dx11Instance::LoadMaterials() {
		for (const auto& mat : model->materialData.materials) {
			Dx11ModelMaterial material(mat);
			if (!mat.texture.empty())
				material.texture = viewer.LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = viewer.LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = viewer.LoadTexture(mat.toonTexture);
			modelResources.materials.emplace_back(std::move(material));
		}
	}

	Dx11Instance::Dx11Instance(Dx11Viewer& sourceViewer) : viewer(sourceViewer) {
		drawer = std::make_unique<Dx11Drawer>(*this, modelResources, viewer.GetDrawContext(), viewer);
	}

	void Dx11Instance::ResetRendererResources() {
		modelResources.materials.clear();
		modelResources.vertexBuffer.Reset();
		modelResources.indexBuffer.Reset();
		modelResources.vsConstantBuffer.Reset();
		modelResources.psConstantBuffer.Reset();
		modelResources.sceneSurfaceConstantBuffer.Reset();
		modelResources.edgeVsConstantBuffer.Reset();
		modelResources.edgePsConstantBuffer.Reset();
		modelResources.gsVsConstantBuffer.Reset();
		modelResources.gsPsConstantBuffer.Reset();
		modelResources.indexBufferFormat = DXGI_FORMAT_R16_UINT;
	}

	bool Dx11Instance::SetupRenderer() {
		if (!CreateGeometryBuffers())
			return false;
		if (!CreateConstantBuffers())
			return false;
		LoadMaterials();
		return true;
	}

	bool Dx11Instance::Upload() {
		ID3D11DeviceContext* deviceContext = viewer.GetDrawContext().GetDeviceContext();
		const size_t vtxCount = model->geometryData.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		if (FAILED(deviceContext->Map(modelResources.vertexBuffer.Get(), 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
			return false;
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(mapRes.pData), vtxCount });
		deviceContext->Unmap(modelResources.vertexBuffer.Get(), 0);
		return writeSucceeded;
	}
}

