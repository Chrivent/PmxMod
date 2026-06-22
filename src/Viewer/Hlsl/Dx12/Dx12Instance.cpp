#include "Dx12Instance.h"

#include "Dx12Drawer.h"
#include "Dx12Viewer.h"
#include "../HlslShaderConstants.h"
#include "../../../Core/Model/Model.h"
#include "../../ViewerGeometry.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	bool Dx12Instance::CreateGeometryBuffers(Dx12InstanceInfo& info, const Dx12DeviceInfo& deviceInfo) {
		const auto& geometryData = info.model->geometryData;
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
		if (vertexByteSize > (std::numeric_limits<UINT>::max)() ||
			indexData.bytes.size() > (std::numeric_limits<UINT>::max)())
			return false;
		for (size_t frameIndex = 0; frameIndex < Dx12InstanceInfo::kBufferedFrames; frameIndex++) {
			Dx12Buffer& vertexBuffer = info.vertexBuffers[frameIndex];
			if (!vertexBuffer.InitializeUpload(deviceInfo, vertexByteSize) ||
				!ViewerGeometry::WriteVertices(geometryData, false,
					static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount))
				return false;
			auto& vertexBufferView = info.vertexBufferViews[frameIndex];
			vertexBufferView.BufferLocation = vertexBuffer.ResolveGpuAddress();
			vertexBufferView.SizeInBytes = vertexByteSize;
			vertexBufferView.StrideInBytes = sizeof(ViewerVertex);
		}
		if (!info.indexBuffer.InitializeUpload(deviceInfo, indexData.bytes.size()) ||
			!info.indexBuffer.Write(indexData.bytes.data(), indexData.bytes.size()))
			return false;
		info.indexBufferView.BufferLocation = info.indexBuffer.ResolveGpuAddress();
		info.indexBufferView.SizeInBytes = indexData.bytes.size();
		info.indexBufferView.Format = indexFormat;
		info.indexCount = indexData.indexCount;
		return true;
	}

	bool Dx12Instance::CreateConstantBuffers(Dx12InstanceInfo& info, const Dx12DeviceInfo& deviceInfo) {
		const size_t vertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslModelVertexConstants));
		const size_t pixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslModelPixelConstants));
		const size_t edgeVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslEdgeVertexConstants));
		const size_t edgeSizeConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslEdgeSizeConstants));
		const size_t edgePixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslEdgePixelConstants));
		const size_t groundShadowVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslGroundShadowVertexConstants));
		const size_t groundShadowPixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslGroundShadowPixelConstants));
		for (size_t frameIndex = 0; frameIndex < Dx12InstanceInfo::kBufferedFrames; frameIndex++) {
			if (!info.modelVertexConstantBuffers[frameIndex].InitializeUpload(deviceInfo, vertexConstantSize) ||
				!info.edgeVertexConstantBuffers[frameIndex].InitializeUpload(deviceInfo, edgeVertexConstantSize) ||
				!info.groundShadowVertexConstantBuffers[frameIndex].InitializeUpload(deviceInfo, groundShadowVertexConstantSize) ||
				!info.groundShadowPixelConstantBuffers[frameIndex].InitializeUpload(deviceInfo, groundShadowPixelConstantSize))
				return false;
		}
		const size_t materialCount = info.model->materialData.materials.size();
		info.modelPixelConstantBuffers.resize(materialCount);
		for (auto& buffers : info.modelPixelConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(Dx12InstanceInfo::kBufferedFrames);
			for (size_t i = 0; i < Dx12InstanceInfo::kBufferedFrames; i++) {
				if (!buffers[i].InitializeUpload(deviceInfo, pixelConstantSize))
					return false;
			}
		}
		info.edgeSizeConstantBuffers.resize(materialCount);
		for (auto& buffers : info.edgeSizeConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(Dx12InstanceInfo::kBufferedFrames);
			for (size_t i = 0; i < Dx12InstanceInfo::kBufferedFrames; i++) {
				if (!buffers[i].InitializeUpload(deviceInfo, edgeSizeConstantSize))
					return false;
			}
		}
		info.edgePixelConstantBuffers.resize(materialCount);
		for (auto& buffers : info.edgePixelConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(Dx12InstanceInfo::kBufferedFrames);
			for (size_t i = 0; i < Dx12InstanceInfo::kBufferedFrames; i++) {
				if (!buffers[i].InitializeUpload(deviceInfo, edgePixelConstantSize))
					return false;
			}
		}
		return true;
	}

	void Dx12Instance::LoadMaterials(Dx12InstanceInfo& info) {
		info.materials.reserve(info.model->materialData.materials.size());
		for (const auto& mat : info.model->materialData.materials) {
			Dx12Material material(mat);
			if (!mat.texture.empty())
				material.texture = info.viewer->LoadTexture(mat.texture);
			if (!mat.spTexture.empty())
				material.sphereTexture = info.viewer->LoadTexture(mat.spTexture);
			if (!mat.toonTexture.empty())
				material.toonTexture = info.viewer->LoadTexture(mat.toonTexture);
			info.materials.emplace_back(material);
		}
	}

	bool Dx12Instance::CreateTextureDescriptors(Dx12InstanceInfo& info) {
		if (info.viewer == nullptr || info.materials.empty())
			return true;
		const auto& viewerInfo = info.viewer->GetDx12Info();
		if (!viewerInfo.deviceInfo || !viewerInfo.dummyTexture)
			return false;
		const auto& deviceInfo = *viewerInfo.deviceInfo;
		if (!deviceInfo.device)
			return false;
		if (info.materials.size() > (std::numeric_limits<UINT>::max)() / 3)
			return false;
		const size_t descriptorCount = info.materials.size() * 3;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = descriptorCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(deviceInfo.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&info.textureDescriptorHeap))))
			return false;
		info.textureDescriptorSize = deviceInfo.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = info.textureDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = info.textureDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		const auto CreateSrv = [&](const Dx12Texture& texture, D3D12_CPU_DESCRIPTOR_HANDLE targetHandle) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = texture.format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			deviceInfo.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, targetHandle);
		};
		for (Dx12Material& material : info.materials) {
			material.textureDescriptorHandle = gpuHandle;
			const Dx12Texture& texture = material.texture.resource ? material.texture : *viewerInfo.dummyTexture;
			const Dx12Texture& toonTexture = material.toonTexture.resource ? material.toonTexture : *viewerInfo.dummyTexture;
			const Dx12Texture& sphereTexture = material.sphereTexture.resource ? material.sphereTexture : *viewerInfo.dummyTexture;
			CreateSrv(texture, cpuHandle);
			cpuHandle.ptr += info.textureDescriptorSize;
			CreateSrv(toonTexture, cpuHandle);
			cpuHandle.ptr += info.textureDescriptorSize;
			CreateSrv(sphereTexture, cpuHandle);
			cpuHandle.ptr += info.textureDescriptorSize;
			gpuHandle.ptr += info.textureDescriptorSize * 3;
		}
		return true;
	}

	Dx12Instance::Dx12Instance() {
		info = std::make_unique<Dx12InstanceInfo>();
		drawer = std::make_unique<Dx12Drawer>(static_cast<Dx12InstanceInfo&>(GetInfo()));
	}

	void Dx12Instance::Clear() {
		auto& info = static_cast<Dx12InstanceInfo&>(GetInfo());
		for (Dx12Buffer& vertexBuffer : info.vertexBuffers)
			vertexBuffer.Destroy();
		info.indexBuffer.Destroy();
		for (Dx12Buffer& buffer : info.modelVertexConstantBuffers)
			buffer.Destroy();
		for (auto& buffers : info.modelPixelConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < Dx12InstanceInfo::kBufferedFrames; i++)
				buffers[i].Destroy();
		}
		info.modelPixelConstantBuffers.clear();
		for (Dx12Buffer& buffer : info.edgeVertexConstantBuffers)
			buffer.Destroy();
		for (auto& buffers : info.edgeSizeConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < Dx12InstanceInfo::kBufferedFrames; i++)
				buffers[i].Destroy();
		}
		info.edgeSizeConstantBuffers.clear();
		for (auto& buffers : info.edgePixelConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < Dx12InstanceInfo::kBufferedFrames; i++)
				buffers[i].Destroy();
		}
		info.edgePixelConstantBuffers.clear();
		for (Dx12Buffer& buffer : info.groundShadowVertexConstantBuffers)
			buffer.Destroy();
		for (Dx12Buffer& buffer : info.groundShadowPixelConstantBuffers)
			buffer.Destroy();
		info.textureDescriptorHeap.Reset();
		info.textureDescriptorSize = 0;
		for (auto& vertexBufferView : info.vertexBufferViews)
			vertexBufferView = {};
		info.indexBufferView = {};
		info.indexCount = 0;
		info.materials.clear();
	}

	bool Dx12Instance::Setup(Viewer& baseViewer) {
		auto& info = static_cast<Dx12InstanceInfo&>(GetInfo());
		Clear();
		info.viewer = static_cast<Dx12Viewer*>(&baseViewer);
		if (info.model == nullptr)
			return false;
		const auto& viewerInfo = info.viewer->GetDx12Info();
		if (!viewerInfo.deviceInfo)
			return false;
		const auto& deviceInfo = *viewerInfo.deviceInfo;
		if (!CreateGeometryBuffers(info, deviceInfo))
			return false;
		if (!CreateConstantBuffers(info, deviceInfo))
			return false;
		LoadMaterials(info);
		return CreateTextureDescriptors(info);
	}

	void Dx12Instance::Upload() const {
		const auto& info = static_cast<const Dx12InstanceInfo&>(GetInfo());
		if (info.model == nullptr || info.viewer == nullptr)
			return;
		const size_t frameIndex = info.viewer->GetDx12Info().frameIndex % Dx12InstanceInfo::kBufferedFrames;
		const Dx12Buffer& vertexBuffer = info.vertexBuffers[frameIndex];
		if (!vertexBuffer.IsInitialized())
			return;
		const size_t vertexCount = info.model->geometryData.positions.size();
		if (!ViewerGeometry::WriteVertices(info.model->geometryData, true,
			static_cast<ViewerVertex*>(vertexBuffer.ResolveMappedData()), vertexCount))
			std::cerr << "Failed to update DX12 vertex buffer.\n";
	}
}
