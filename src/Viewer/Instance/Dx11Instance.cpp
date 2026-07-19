#include "Viewer/Instance/Dx11Instance.h"

#include "Viewer/Drawer/Dx11Drawer.h"

#include "Viewer/Buffer/BufferSize.h"
#include "Viewer/Device/Dx11Device.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/Texture/Dx11TextureCache.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Core/Model/Model.h"

#include <limits>
#include <utility>

namespace Chrivent {
	GraphicsError::Result<void> Dx11Instance::CreateGeometryBuffers() {
		ID3D11Device* targetDevice = device.GetDevice();
		if (targetDevice == nullptr) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX11 geometry 생성", "DirectX 11 device를 사용할 수 없습니다"));
		}
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX11 geometry 생성", "모델 index 데이터를 만들지 못했습니다"));
		}
		size_t vertexByteSize = 0;
		if (!BufferSize::TryMultiply(
			sizeof(ViewerVertex), geometryData.positions.size(), vertexByteSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX11 geometry 생성", "vertex buffer 크기가 한도를 넘습니다"));
		}
		if (vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max()) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX11 geometry 생성", "vertex 또는 index 데이터가 DirectX 11 크기 범위를 벗어났습니다"));
		}
		const auto vBufDesc = Dx11DescBuilder::MakeDynamicVertexBufferDesc(static_cast<UINT>(vertexByteSize));
		HRESULT result = targetDevice->CreateBuffer(
			&vBufDesc, nullptr, &modelResources.vertexBuffer);
		if (FAILED(result)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX11 vertex buffer 생성", "동적 vertex buffer를 만들지 못했습니다", result, true));
		}
		const auto iBufDesc = Dx11DescBuilder::MakeImmutableIndexBufferDesc(static_cast<UINT>(indexData.bytes.size()));
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = indexData.bytes.data();
		result = targetDevice->CreateBuffer(
			&iBufDesc, &initData, &modelResources.indexBuffer);
		if (FAILED(result)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX11 index buffer 생성", "index buffer를 만들지 못했습니다", result, true));
		}
		if (indexData.elementSize == sizeof(uint16_t))
			modelResources.indexBufferFormat = DXGI_FORMAT_R16_UINT;
		else if (indexData.elementSize == sizeof(uint32_t))
			modelResources.indexBufferFormat = DXGI_FORMAT_R32_UINT;
		else {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX11 geometry 생성", "index element 크기가 올바르지 않습니다"));
		}
		return {};
	}

	GraphicsError::Result<void> Dx11Instance::CreateConstantBuffers() {
		ID3D11Device* targetDevice = device.GetDevice();
		if (targetDevice == nullptr) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX11 constant buffer 생성", "DirectX 11 device를 사용할 수 없습니다"));
		}
		HRESULT result = CreateBuffer<ModelVertexConstants>(
			targetDevice, modelResources.vsConstantBuffer);
		if (SUCCEEDED(result))
			result = CreateBuffer<ModelPixelConstants>(
				targetDevice, modelResources.psConstantBuffer);
		if (SUCCEEDED(result))
			result = CreateBuffer<SceneSurfacePixelConstants>(
				targetDevice, modelResources.sceneSurfaceConstantBuffer);
		if (SUCCEEDED(result))
			result = CreateBuffer<EdgeVertexConstants>(
				targetDevice, modelResources.edgeVsConstantBuffer);
		if (SUCCEEDED(result))
			result = CreateBuffer<EdgePixelConstants>(
				targetDevice, modelResources.edgePsConstantBuffer);
		if (SUCCEEDED(result))
			result = CreateBuffer<GroundShadowVertexConstants>(
				targetDevice, modelResources.gsVsConstantBuffer);
		if (SUCCEEDED(result))
			result = CreateBuffer<GroundShadowPixelConstants>(
				targetDevice, modelResources.gsPsConstantBuffer);
		if (FAILED(result)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX11 constant buffer 생성", "패스별 constant buffer를 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> Dx11Instance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			Dx11ModelMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto textureResult = textureCache.Load(device.GetDevice(), mat.texture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.texture = **textureResult;
			}
			if (!mat.spTexture.empty()) {
				const auto textureResult = textureCache.Load(device.GetDevice(), mat.spTexture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.sphereTexture = **textureResult;
			}
			if (!mat.toonTexture.empty()) {
				const auto textureResult = textureCache.Load(device.GetDevice(), mat.toonTexture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.toonTexture = **textureResult;
			}
			modelResources.materials.emplace_back(std::move(material));
		}
		return {};
	}

	Dx11Instance::Dx11Instance(const Dx11Device& sourceDevice,
		Dx11TextureCache& sourceTextureCache, Dx11DrawContext& sourceDrawContext)
		: Instance(GraphicsApi::DirectX11), device(sourceDevice),
		textureCache(sourceTextureCache), drawContext(sourceDrawContext) {
		drawer = std::make_unique<Dx11Drawer>(*this, modelResources, drawContext);
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

	GraphicsError::Result<void> Dx11Instance::SetupRenderer() {
		auto result = CreateGeometryBuffers();
		if (!result)
			return std::unexpected(result.error());
		result = CreateConstantBuffers();
		if (!result)
			return std::unexpected(result.error());
		return LoadMaterials();
	}

	GraphicsError::Result<void> Dx11Instance::UploadCore() {
		ID3D11DeviceContext* deviceContext = device.GetContext();
		const size_t vtxCount = model->geometryData.positions.size();
		D3D11_MAPPED_SUBRESOURCE mapRes;
		const HRESULT mapResult = deviceContext->Map(
			modelResources.vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapRes);
		if (FAILED(mapResult))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX11 모델 정점 업로드", "vertex buffer를 매핑하지 못했습니다",
				mapResult, true));
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(mapRes.pData), vtxCount });
		deviceContext->Unmap(modelResources.vertexBuffer.Get(), 0);
		if (!writeSucceeded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX11 모델 정점 업로드", "vertex 데이터를 기록하지 못했습니다"));
		return {};
	}
}

