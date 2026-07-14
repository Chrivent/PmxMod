#include "Viewer/Dx12/Dx12Instance.h"

#include "Viewer/Dx12/Dx12Drawer.h"
#include "Viewer/Dx12/Dx12Viewer.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Core/Model/Model.h"
#include "Viewer/ViewerGeometry.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace Chrivent {
	bool Dx12Instance::CreateGeometryBuffers(const Dx12Device& device) {
		const auto& geometryData = model->geometryData;
		ViewerIndexData indexData;
		if (geometryData.positions.empty() ||
			!ViewerGeometry::BuildIndexData(geometryData, indexData) ||
			indexData.bytes.empty()) {
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
		for (size_t frameIndex = 0; frameIndex < kBufferedFrames; frameIndex++) {
			Dx12Buffer& vertexBuffer = vertexBuffers[frameIndex];
			if (!vertexBuffer.InitializeUpload(device, vertexByteSize) ||
				!ViewerGeometry::WriteVertices(geometryData, false,
					{ static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount }))
				return false;
			auto& [BufferLocation, SizeInBytes, StrideInBytes] = vertexBufferViews[frameIndex];
			BufferLocation = vertexBuffer.ResolveGpuAddress();
			SizeInBytes = vertexByteSize;
			StrideInBytes = sizeof(ViewerVertex);
		}
		if (!indexBuffer.InitializeUpload(device, indexData.bytes.size()) ||
			!indexBuffer.Write(std::as_bytes(std::span(indexData.bytes))))
			return false;
		indexBufferView.BufferLocation = indexBuffer.ResolveGpuAddress();
		indexBufferView.SizeInBytes = indexData.bytes.size();
		indexBufferView.Format = indexFormat;
		indexCount = indexData.indexCount;
		return true;
	}

	bool Dx12Instance::CreateConstantBuffers(const Dx12Device& device) {
		const size_t modelVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelVertexConstants));
		const size_t postProcessVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(
			std::max(sizeof(ModelVertexConstants), sizeof(SceneVelocityVertexConstants)));
		const size_t vertexConstantSize = modelVertexConstantSize + postProcessVertexConstantSize;
		const size_t pixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelPixelConstants));
		const size_t edgeVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(EdgeVertexConstants));
		const size_t edgePixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(EdgePixelConstants));
		const size_t groundShadowVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(GroundShadowVertexConstants));
		const size_t groundShadowPixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(GroundShadowPixelConstants));
		for (size_t frameIndex = 0; frameIndex < kBufferedFrames; frameIndex++) {
			if (!modelVertexConstantBuffers[frameIndex].InitializeUpload(device, vertexConstantSize) ||
				!groundShadowVertexConstantBuffers[frameIndex].InitializeUpload(device, groundShadowVertexConstantSize) ||
				!groundShadowPixelConstantBuffers[frameIndex].InitializeUpload(device, groundShadowPixelConstantSize))
				return false;
		}
		const size_t materialCount = model->materialData.materials.size();
		modelPixelConstantBuffers.resize(materialCount);
		for (auto& buffers : modelPixelConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(kBufferedFrames);
			for (size_t i = 0; i < kBufferedFrames; i++) {
				if (!buffers[i].InitializeUpload(device, pixelConstantSize))
					return false;
			}
		}
		edgeVertexConstantBuffers.resize(materialCount);
		for (auto& buffers : edgeVertexConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(kBufferedFrames);
			for (size_t i = 0; i < kBufferedFrames; i++) {
				if (!buffers[i].InitializeUpload(device, edgeVertexConstantSize))
					return false;
			}
		}
		edgePixelConstantBuffers.resize(materialCount);
		for (auto& buffers : edgePixelConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(kBufferedFrames);
			for (size_t i = 0; i < kBufferedFrames; i++) {
				if (!buffers[i].InitializeUpload(device, edgePixelConstantSize))
					return false;
			}
		}
		return true;
	}

	void Dx12Instance::LoadMaterials() {
		materials.reserve(model->materialData.materials.size());
		for (const auto& mat : model->materialData.materials) {
			Dx12Material material(mat);
			if (!mat.texture.empty())
				material.texture = viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = viewer->LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = viewer->LoadTexture(mat.toonTexture);
			materials.emplace_back(material);
		}
	}

	bool Dx12Instance::CreateTextureDescriptors() {
		if (viewer == nullptr || materials.empty())
			return true;
		if (!viewer->device || !viewer->dummyTexture)
			return false;
		const auto& device = *viewer->device;
		if (!device.device)
			return false;
		if (materials.size() > std::numeric_limits<UINT>::max() / 3)
			return false;
		const size_t descriptorCount = materials.size() * 3;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = descriptorCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(device.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&textureDescriptorHeap))))
			return false;
		textureDescriptorSize = device.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = textureDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = textureDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		const auto CreateSrv = [&](const Dx12Texture& texture, D3D12_CPU_DESCRIPTOR_HANDLE targetHandle) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = texture.format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			device.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, targetHandle);
		};
		for (Dx12Material& material : materials) {
			material.textureDescriptorHandle = gpuHandle;
			const Dx12Texture& texture = material.texture.resource ? material.texture : *viewer->dummyTexture;
			const Dx12Texture& toonTexture = material.toonTexture.resource ? material.toonTexture : *viewer->dummyTexture;
			const Dx12Texture& sphereTexture = material.sphereTexture.resource ? material.sphereTexture : *viewer->dummyTexture;
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

	Dx12Instance::Dx12Instance() { drawer = std::make_unique<Dx12Drawer>(*this); }

	void Dx12Instance::Clear() {
		for (Dx12Buffer& vertexBuffer : vertexBuffers)
			vertexBuffer.Reset();
		indexBuffer.Reset();
		for (Dx12Buffer& buffer : modelVertexConstantBuffers)
			buffer.Reset();
		for (auto& buffers : modelPixelConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < kBufferedFrames; i++)
				buffers[i].Reset();
		}
		modelPixelConstantBuffers.clear();
		for (auto& buffers : edgeVertexConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < kBufferedFrames; i++)
				buffers[i].Reset();
		}
		edgeVertexConstantBuffers.clear();
		for (auto& buffers : edgePixelConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < kBufferedFrames; i++)
				buffers[i].Reset();
		}
		edgePixelConstantBuffers.clear();
		for (Dx12Buffer& buffer : groundShadowVertexConstantBuffers)
			buffer.Reset();
		for (Dx12Buffer& buffer : groundShadowPixelConstantBuffers)
			buffer.Reset();
		textureDescriptorHeap.Reset();
		textureDescriptorSize = 0;
		for (auto& vertexBufferView : vertexBufferViews)
			vertexBufferView = {};
		indexBufferView = {};
		indexCount = 0;
		materials.clear();
	}

	bool Dx12Instance::Setup(Viewer& baseViewer) {
		Clear();
		viewer = static_cast<Dx12Viewer*>(&baseViewer);
		if (model == nullptr)
			return false;
		if (!viewer->device)
			return false;
		const auto& device = *viewer->device;
		if (!CreateGeometryBuffers(device))
			return false;
		if (!CreateConstantBuffers(device))
			return false;
		LoadMaterials();
		return CreateTextureDescriptors();
	}

	void Dx12Instance::Upload() const {
		if (model == nullptr || viewer == nullptr)
			return;
		const size_t frameIndex = viewer->frameIndex % kBufferedFrames;
		const Dx12Buffer& vertexBuffer = vertexBuffers[frameIndex];
		if (!vertexBuffer.IsInitialized())
			return;
		const size_t vertexCount = model->geometryData.positions.size();
		if (!ViewerGeometry::WriteVertices(model->geometryData, true,
			{ static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount }))
			std::cerr << "Failed to update DX12 vertex buffer.\n";
	}
}
