#include "Viewer/Instance/Dx11Instance.h"

#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/Device/Dx11Device.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/Texture/Dx11TextureCache.h"
#include "Viewer/Viewer/Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

#include <limits>
#include <utility>

namespace Chrivent {
	bool Dx11Instance::CreateGeometryBuffers() {
		ID3D11Device* targetDevice = device.GetDevice();
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData))
			return false;
		const size_t vertexByteSize = sizeof(ViewerVertex) * geometryData.positions.size();
		if (vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max())
			return false;
		const auto vBufDesc = Dx11DescBuilder::MakeDynamicVertexBufferDesc(static_cast<UINT>(vertexByteSize));
		if (FAILED(targetDevice->CreateBuffer(
			&vBufDesc, nullptr, &modelResources.vertexBuffer)))
			return false;
		const auto iBufDesc = Dx11DescBuilder::MakeImmutableIndexBufferDesc(static_cast<UINT>(indexData.bytes.size()));
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = indexData.bytes.data();
		if (FAILED(targetDevice->CreateBuffer(
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
		ID3D11Device* targetDevice = device.GetDevice();
		if (FAILED(CreateBuffer<ModelVertexConstants>(
			targetDevice, modelResources.vsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<ModelPixelConstants>(
			targetDevice, modelResources.psConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<SceneSurfacePixelConstants>(
			targetDevice, modelResources.sceneSurfaceConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgeVertexConstants>(
			targetDevice, modelResources.edgeVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<EdgePixelConstants>(
			targetDevice, modelResources.edgePsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowVertexConstants>(
			targetDevice, modelResources.gsVsConstantBuffer)))
			return false;
		if (FAILED(CreateBuffer<GroundShadowPixelConstants>(
			targetDevice, modelResources.gsPsConstantBuffer)))
			return false;
		return true;
	}

	void Dx11Instance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			Dx11ModelMaterial material(mat);
			if (!mat.texture.empty())
				material.texture = textureCache.Load(device.GetDevice(), mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = textureCache.Load(device.GetDevice(), mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = textureCache.Load(device.GetDevice(), mat.toonTexture);
			modelResources.materials.emplace_back(std::move(material));
		}
	}

	Dx11Instance::Dx11Instance(Viewer& sourceViewer, const Dx11Device& sourceDevice,
		Dx11TextureCache& sourceTextureCache, const Dx11DrawContext& sourceDrawContext)
		: device(sourceDevice), textureCache(sourceTextureCache), drawContext(sourceDrawContext) {
		drawer = std::make_unique<Dx11Drawer>(*this, modelResources, drawContext, sourceViewer);
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
		ID3D11DeviceContext* deviceContext = device.GetContext();
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

