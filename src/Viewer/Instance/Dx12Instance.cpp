#include "Viewer/Instance/Dx12Instance.h"

#include "Viewer/Drawer/Dx12Drawer.h"
#include "Viewer/Buffer/BufferSize.h"
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
		size_t vertexByteSize = 0;
		if (!BufferSize::TryMultiply(sizeof(ViewerVertex), vertexCount, vertexByteSize)
			|| vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max()) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 geometry 생성", "vertex 또는 index 데이터가 DirectX 12 크기 범위를 벗어났습니다"));
		}
		for (size_t frameIndex = 0; frameIndex < FrameBuffering::dx12BufferCount; frameIndex++) {
			Dx12Buffer& vertexBuffer = modelResources.vertexBuffers[frameIndex];
			const auto bufferResult = vertexBuffer.InitializeUpload(device, vertexByteSize);
			if (!bufferResult)
				return std::unexpected(bufferResult.error());
			if (!ViewerGeometry::WriteVertices(geometryData, false,
				{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount })) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"DX12 vertex buffer 기록", "초기 vertex 데이터를 기록하지 못했습니다"));
			}
			auto& [BufferLocation, SizeInBytes, StrideInBytes] = modelResources.vertexBufferViews[frameIndex];
			BufferLocation = vertexBuffer.GetGpuAddress();
			SizeInBytes = vertexByteSize;
			StrideInBytes = sizeof(ViewerVertex);
		}
		Dx12Buffer indexUploadBuffer;
		auto bufferResult = indexUploadBuffer.InitializeUpload(device, indexData.bytes.size());
		if (!bufferResult)
			return std::unexpected(bufferResult.error());
		if (!indexUploadBuffer.Write(std::as_bytes(std::span(indexData.bytes)))) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"DX12 index upload buffer 기록", "index 데이터를 기록하지 못했습니다"));
		}
		bufferResult = modelResources.indexBuffer.InitializeDefault(
			device, indexData.bytes.size(), D3D12_RESOURCE_STATE_COPY_DEST);
		if (!bufferResult)
			return std::unexpected(bufferResult.error());
		const auto uploadResult = uploadContext.RecordIndexBufferUpload(
			modelResources.indexBuffer.GetResource(), indexUploadBuffer.GetResource(), indexData.bytes.size());
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		modelResources.indexBufferView.BufferLocation = modelResources.indexBuffer.GetGpuAddress();
		modelResources.indexBufferView.SizeInBytes = indexData.bytes.size();
		modelResources.indexBufferView.Format = indexFormat;
		modelResources.indexCount = indexData.indexCount;
		return {};
	}

	GraphicsResult<void> Dx12Instance::CreateConstantBuffers() {
		auto& [modelVertex, sceneInputVertex, groundShadowVertex, groundShadowPixel
			, materialBase, materialStride, modelPixel, sceneSurfacePixel
			, edgeVertex, edgePixel, totalByteSize] = modelResources.constantBufferLayout;
		size_t frameOffset = 0;
		const auto ReserveFrameConstants = [&frameOffset](const size_t size, size_t& offset) {
			offset = frameOffset;
			size_t alignedSize = 0;
			return Dx12Buffer::TryAlignConstantBufferSize(size, alignedSize)
				&& BufferSize::TryAdd(frameOffset, alignedSize, frameOffset);
		};
		if (!ReserveFrameConstants(sizeof(ModelVertexConstants), modelVertex)
			|| !ReserveFrameConstants(
				std::max(sizeof(ModelVertexConstants), sizeof(SceneVelocityVertexConstants)), sceneInputVertex)
			|| !ReserveFrameConstants(sizeof(GroundShadowVertexConstants), groundShadowVertex)
			|| !ReserveFrameConstants(sizeof(GroundShadowPixelConstants), groundShadowPixel)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 constant buffer layout 생성", "frame constant buffer 크기가 한도를 넘습니다"));
		}
		materialBase = frameOffset;
		size_t materialOffset = 0;
		const auto ReserveMaterialConstants = [&materialOffset](const size_t size, size_t& offset) {
			offset = materialOffset;
			size_t alignedSize = 0;
			return Dx12Buffer::TryAlignConstantBufferSize(size, alignedSize)
				&& BufferSize::TryAdd(materialOffset, alignedSize, materialOffset);
		};
		if (!ReserveMaterialConstants(sizeof(ModelPixelConstants), modelPixel)
			|| !ReserveMaterialConstants(sizeof(SceneSurfacePixelConstants), sceneSurfacePixel)
			|| !ReserveMaterialConstants(sizeof(EdgeVertexConstants), edgeVertex)
			|| !ReserveMaterialConstants(sizeof(EdgePixelConstants), edgePixel)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 constant buffer layout 생성", "material constant buffer 크기가 한도를 넘습니다"));
		}
		materialStride = materialOffset;
		const size_t materialCount = model->materialData.materials.size();
		size_t materialByteSize = 0;
		if (!BufferSize::TryMultiply(materialCount, materialStride, materialByteSize)
			|| !BufferSize::TryAdd(frameOffset, materialByteSize, totalByteSize)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 constant buffer 크기 계산", "material 수에 따른 constant buffer 크기가 한도를 넘습니다"));
		}
		for (Dx12Buffer& buffer : modelResources.constantBuffers) {
			const auto result = buffer.InitializeUpload(device, totalByteSize);
			if (!result)
				return std::unexpected(result.error());
		}
		return {};
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

	GraphicsResult<void> Dx12Instance::CreateTextureDescriptors() {
		if (modelResources.materials.empty())
			return {};
		if (!device.GetDevice()) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"DX12 texture descriptor 생성", "DirectX 12 device를 사용할 수 없습니다"));
		}
		if (modelResources.materials.size() > std::numeric_limits<UINT>::max() / 3) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"DX12 texture descriptor 생성", "material 수가 descriptor 개수 범위를 벗어났습니다"));
		}
		const size_t descriptorCount = modelResources.materials.size() * 3;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = static_cast<UINT>(descriptorCount);
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		const HRESULT result = device.GetDevice()->CreateDescriptorHeap(
			&heapDesc, IID_PPV_ARGS(&modelResources.textureDescriptorHeap));
		if (FAILED(result)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"DX12 texture descriptor heap 생성",
				"material texture descriptor heap을 만들지 못했습니다", result, true));
		}
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
		return {};
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
		const auto beginUploadResult = textureCache.BeginUploadBatch(device);
		if (!beginUploadResult)
			return std::unexpected(beginUploadResult.error());
		const auto geometryResult = CreateGeometryBuffers();
		if (!geometryResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(geometryResult.error());
		}
		const auto constantBufferResult = CreateConstantBuffers();
		if (!constantBufferResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(constantBufferResult.error());
		}
		const auto materialResult = LoadMaterials();
		if (!materialResult) {
			textureCache.CancelUploadBatch();
			return std::unexpected(materialResult.error());
		}
		const auto uploadResult = textureCache.SubmitUploadBatch(device);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		const auto descriptorResult = CreateTextureDescriptors();
		if (!descriptorResult)
			return std::unexpected(descriptorResult.error());
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
