#include "Viewer/Instance/Dx12Instance.h"

#include "Viewer/Drawer/Dx12Drawer.h"
#include "Viewer/DrawContext/Dx12DrawContext.h"
#include "Viewer/Command/Dx12UploadContext.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Texture/Dx12TextureCache.h"
#include "Core/Model/Model.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Chrivent {
	GraphicsResult<void> Dx12Instance::CreateGeometryBuffers() {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 geometry 생성", "모델 index 데이터를 만들지 못했습니다"));
		}
		const DXGI_FORMAT indexFormat = indexData.elementSize == sizeof(uint16_t)
			? DXGI_FORMAT_R16_UINT
			: DXGI_FORMAT_R32_UINT;
		const size_t vertexCount = geometryData.positions.size();
		const size_t vertexByteSize = sizeof(ViewerVertex) * vertexCount;
		if (vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max()) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 geometry 생성", "vertex 또는 index 데이터가 DirectX 12 크기 범위를 벗어났습니다"));
		}
		for (size_t frameIndex = 0; frameIndex < FrameBuffering::dx12BufferCount; frameIndex++) {
			Dx12Buffer& vertexBuffer = modelResources.vertexBuffers[frameIndex];
			if (!vertexBuffer.InitializeUpload(device, vertexByteSize) ||
				!ViewerGeometry::WriteVertices(geometryData, false,
					{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount })) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"DX12 vertex buffer 생성", "동적 vertex buffer를 만들거나 기록하지 못했습니다"));
			}
			auto& [BufferLocation, SizeInBytes, StrideInBytes] = modelResources.vertexBufferViews[frameIndex];
			BufferLocation = vertexBuffer.GetGpuAddress();
			SizeInBytes = vertexByteSize;
			StrideInBytes = sizeof(ViewerVertex);
		}
		Dx12Buffer indexUploadBuffer;
		if (!indexUploadBuffer.InitializeUpload(device, indexData.bytes.size())
			|| !indexUploadBuffer.Write(std::as_bytes(std::span(indexData.bytes)))
			|| !modelResources.indexBuffer.InitializeDefault(
				device, indexData.bytes.size(), D3D12_RESOURCE_STATE_COPY_DEST)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX12 index buffer 생성", "index upload 또는 GPU buffer를 만들지 못했습니다"));
		}
		const auto uploadResult = uploadContext.UploadIndexBuffer(
			device, modelResources.indexBuffer.GetResource(),
			indexUploadBuffer.GetResource(), indexData.bytes.size());
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		modelResources.indexBufferView.BufferLocation = modelResources.indexBuffer.GetGpuAddress();
		modelResources.indexBufferView.SizeInBytes = indexData.bytes.size();
		modelResources.indexBufferView.Format = indexFormat;
		modelResources.indexCount = indexData.indexCount;
		return {};
	}

	bool Dx12Instance::CreateConstantBuffers() {
		auto& [modelVertex, sceneInputVertex, groundShadowVertex, groundShadowPixel
			, materialBase, materialStride, modelPixel, sceneSurfacePixel
			, edgeVertex, edgePixel, totalByteSize] = modelResources.constantBufferLayout;
		size_t frameOffset = 0;
		const auto ReserveFrameConstants = [&frameOffset](const size_t size) {
			const size_t offset = frameOffset;
			frameOffset += Dx12Buffer::AlignConstantBufferSize(size);
			return offset;
		};
		modelVertex = ReserveFrameConstants(sizeof(ModelVertexConstants));
		sceneInputVertex = ReserveFrameConstants(
			std::max(sizeof(ModelVertexConstants), sizeof(SceneVelocityVertexConstants)));
		groundShadowVertex = ReserveFrameConstants(sizeof(GroundShadowVertexConstants));
		groundShadowPixel = ReserveFrameConstants(sizeof(GroundShadowPixelConstants));
		materialBase = frameOffset;
		size_t materialOffset = 0;
		const auto ReserveMaterialConstants = [&materialOffset](const size_t size) {
			const size_t offset = materialOffset;
			materialOffset += Dx12Buffer::AlignConstantBufferSize(size);
			return offset;
		};
		modelPixel = ReserveMaterialConstants(sizeof(ModelPixelConstants));
		sceneSurfacePixel = ReserveMaterialConstants(sizeof(SceneSurfacePixelConstants));
		edgeVertex = ReserveMaterialConstants(sizeof(EdgeVertexConstants));
		edgePixel = ReserveMaterialConstants(sizeof(EdgePixelConstants));
		materialStride = materialOffset;
		const size_t materialCount = model->materialData.materials.size();
		if (materialCount > (std::numeric_limits<size_t>::max() - frameOffset) / materialStride)
			return false;
		totalByteSize = frameOffset + materialCount * materialStride;
		for (Dx12Buffer& buffer : modelResources.constantBuffers) {
			if (!buffer.InitializeUpload(device, totalByteSize))
				return false;
		}
		return true;
	}

	GraphicsResult<void> Dx12Instance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			Dx12ModelMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto textureResult = textureCache.Load(device, mat.texture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.texture = **textureResult;
			}
			if (!mat.spTexture.empty()) {
				const auto textureResult = textureCache.Load(device, mat.spTexture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.sphereTexture = **textureResult;
			}
			if (!mat.toonTexture.empty()) {
				const auto textureResult = textureCache.Load(device, mat.toonTexture);
				if (!textureResult)
					return std::unexpected(textureResult.error());
				if (*textureResult)
					material.toonTexture = **textureResult;
			}
			modelResources.materials.emplace_back(std::move(material));
		}
		return {};
	}

	bool Dx12Instance::CreateTextureDescriptors() {
		if (modelResources.materials.empty())
			return true;
		if (!device.GetDevice())
			return false;
		if (modelResources.materials.size() > std::numeric_limits<UINT>::max() / 3)
			return false;
		const size_t descriptorCount = modelResources.materials.size() * 3;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = descriptorCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(device.GetDevice()->CreateDescriptorHeap(
			&heapDesc, IID_PPV_ARGS(&modelResources.textureDescriptorHeap))))
			return false;
		const UINT textureDescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
			modelResources.textureDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
			modelResources.textureDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		const auto CreateSrv = [&](const Dx12Texture& texture, D3D12_CPU_DESCRIPTOR_HANDLE targetHandle) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = texture.format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			device.GetDevice()->CreateShaderResourceView(texture.resource.Get(), &srvDesc, targetHandle);
		};
		for (Dx12ModelMaterial& material : modelResources.materials) {
			material.textureDescriptorHandle = gpuHandle;
			const Dx12Texture& texture = material.texture.resource ? material.texture : dummyTexture;
			const Dx12Texture& toonTexture = material.toonTexture.resource ? material.toonTexture : dummyTexture;
			const Dx12Texture& sphereTexture = material.sphereTexture.resource ? material.sphereTexture : dummyTexture;
			CreateSrv(texture, cpuHandle);
			cpuHandle.ptr += textureDescriptorSize;
			CreateSrv(toonTexture, cpuHandle);
			cpuHandle.ptr += textureDescriptorSize;
			CreateSrv(sphereTexture, cpuHandle);
			cpuHandle.ptr += textureDescriptorSize;
			gpuHandle.ptr += textureDescriptorSize * 3;
		}
		return true;
	}

	Dx12Instance::Dx12Instance(const Dx12Device& sourceDevice,
		Dx12UploadContext& sourceUploadContext, Dx12TextureCache& sourceTextureCache,
		const Dx12Texture& sourceDummyTexture, Dx12DrawContext& sourceDrawContext)
		: Instance(GraphicsApi::DirectX12), device(sourceDevice),
		uploadContext(sourceUploadContext), textureCache(sourceTextureCache),
		dummyTexture(sourceDummyTexture), drawContext(sourceDrawContext) {
		drawer = std::make_unique<Dx12Drawer>(*this, modelResources, drawContext);
	}

	void Dx12Instance::ResetRendererResources() {
		for (Dx12Buffer& vertexBuffer : modelResources.vertexBuffers)
			vertexBuffer.Reset();
		modelResources.indexBuffer.Reset();
		for (Dx12Buffer& buffer : modelResources.constantBuffers)
			buffer.Reset();
		modelResources.constantBufferLayout = {};
		modelResources.textureDescriptorHeap.Reset();
		for (auto& vertexBufferView : modelResources.vertexBufferViews)
			vertexBufferView = {};
		modelResources.indexBufferView = {};
		modelResources.indexCount = 0;
		modelResources.materials.clear();
	}

	GraphicsResult<void> Dx12Instance::SetupRenderer() {
		const auto geometryResult = CreateGeometryBuffers();
		if (!geometryResult)
			return std::unexpected(geometryResult.error());
		if (!CreateConstantBuffers())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX12 모델 인스턴스 초기화", "constant buffer를 만들지 못했습니다"));
		const auto beginUploadResult = textureCache.BeginUploadBatch(device);
		if (!beginUploadResult)
			return std::unexpected(beginUploadResult.error());
		const auto materialResult = LoadMaterials();
		if (!materialResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(materialResult.error());
		}
		const auto uploadResult = textureCache.SubmitUploadBatch(device);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		if (!CreateTextureDescriptors())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX12 모델 인스턴스 초기화", "texture descriptor를 만들지 못했습니다"));
		return {};
	}

	GraphicsResult<void> Dx12Instance::UploadCore() {
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const Dx12Buffer& vertexBuffer = modelResources.vertexBuffers[frameIndex];
		if (!vertexBuffer.IsInitialized())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX12 모델 정점 업로드", "현재 프레임의 vertex buffer가 초기화되지 않았습니다"));
		const size_t vertexCount = model->geometryData.positions.size();
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount });
		if (!writeSucceeded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX12 모델 정점 업로드", "vertex 데이터를 기록하지 못했습니다"));
		return {};
	}
}
