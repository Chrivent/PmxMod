#include "Viewer/Instance/Dx12Instance.h"

#include "Viewer/Drawer/Dx12Drawer.h"
#include "Viewer/DrawContext/Dx12DrawContext.h"
#include "Viewer/Command/Dx12UploadContext.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Texture/Dx12TextureCache.h"
#include "Viewer/Viewer/Viewer.h"
#include "Core/Model/Model.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

namespace Chrivent {
	bool Dx12Instance::CreateGeometryBuffers() {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (!ViewerGeometry::BuildIndexData(geometryData, indexData)) {
			std::cerr << "Failed to create DX12 model buffers: model has no geometry data.\n";
			return false;
		}
		const DXGI_FORMAT indexFormat = indexData.elementSize == sizeof(uint16_t)
			? DXGI_FORMAT_R16_UINT
			: DXGI_FORMAT_R32_UINT;
		const size_t vertexCount = geometryData.positions.size();
		const size_t vertexByteSize = sizeof(ViewerVertex) * vertexCount;
		if (vertexByteSize > std::numeric_limits<UINT>::max() ||
			indexData.bytes.size() > std::numeric_limits<UINT>::max())
			return false;
		for (size_t frameIndex = 0; frameIndex < FrameBuffering::dx12BufferCount; frameIndex++) {
			Dx12Buffer& vertexBuffer = modelResources.vertexBuffers[frameIndex];
			if (!vertexBuffer.InitializeUpload(device, vertexByteSize) ||
				!ViewerGeometry::WriteVertices(geometryData, false,
					{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount }))
				return false;
			auto& [BufferLocation, SizeInBytes, StrideInBytes] = modelResources.vertexBufferViews[frameIndex];
			BufferLocation = vertexBuffer.GetGpuAddress();
			SizeInBytes = vertexByteSize;
			StrideInBytes = sizeof(ViewerVertex);
		}
		Dx12Buffer indexUploadBuffer;
		if (!indexUploadBuffer.InitializeUpload(device, indexData.bytes.size())
			|| !indexUploadBuffer.Write(std::as_bytes(std::span(indexData.bytes)))
			|| !modelResources.indexBuffer.InitializeDefault(
				device, indexData.bytes.size(), D3D12_RESOURCE_STATE_COPY_DEST)
			|| !uploadContext.UploadIndexBuffer(device, modelResources.indexBuffer.GetResource(),
				indexUploadBuffer.GetResource(), indexData.bytes.size()))
			return false;
		modelResources.indexBufferView.BufferLocation = modelResources.indexBuffer.GetGpuAddress();
		modelResources.indexBufferView.SizeInBytes = indexData.bytes.size();
		modelResources.indexBufferView.Format = indexFormat;
		modelResources.indexCount = indexData.indexCount;
		return true;
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

	void Dx12Instance::LoadMaterials() {
		modelResources.materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			Dx12ModelMaterial material(mat);
			if (!mat.texture.empty())
				material.texture = textureCache.Load(device, mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = textureCache.Load(device, mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = textureCache.Load(device, mat.toonTexture);
			modelResources.materials.emplace_back(std::move(material));
		}
	}

	bool Dx12Instance::CreateTextureDescriptors() {
		if (modelResources.materials.empty())
			return true;
		if (!device.device)
			return false;
		if (modelResources.materials.size() > std::numeric_limits<UINT>::max() / 3)
			return false;
		const size_t descriptorCount = modelResources.materials.size() * 3;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = descriptorCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(device.device->CreateDescriptorHeap(
			&heapDesc, IID_PPV_ARGS(&modelResources.textureDescriptorHeap))))
			return false;
		const UINT textureDescriptorSize = device.device->GetDescriptorHandleIncrementSize(
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
			device.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, targetHandle);
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

	Dx12Instance::Dx12Instance(Viewer& sourceViewer, const Dx12Device& sourceDevice,
		Dx12UploadContext& sourceUploadContext, Dx12TextureCache& sourceTextureCache,
		const Dx12Texture& sourceDummyTexture, Dx12DrawContext& sourceDrawContext)
		: device(sourceDevice), uploadContext(sourceUploadContext), textureCache(sourceTextureCache),
		dummyTexture(sourceDummyTexture), drawContext(sourceDrawContext) {
		drawer = std::make_unique<Dx12Drawer>(*this, modelResources, drawContext, sourceViewer);
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

	bool Dx12Instance::SetupRenderer() {
		if (!CreateGeometryBuffers())
			return false;
		if (!CreateConstantBuffers())
			return false;
		LoadMaterials();
		return CreateTextureDescriptors();
	}

	bool Dx12Instance::Upload() {
		const size_t frameIndex = drawContext.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const Dx12Buffer& vertexBuffer = modelResources.vertexBuffers[frameIndex];
		if (!vertexBuffer.IsInitialized())
			return false;
		const size_t vertexCount = model->geometryData.positions.size();
		const bool writeSucceeded = ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.GetMappedData()), vertexCount });
		if (!writeSucceeded)
			std::cerr << "Failed to update DX12 vertex buffer.\n";
		return writeSucceeded;
	}
}
