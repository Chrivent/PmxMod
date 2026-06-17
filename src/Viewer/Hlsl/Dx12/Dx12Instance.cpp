#include "Dx12Instance.h"

#include "Dx12Drawer.h"
#include "Dx12Viewer.h"
#include "../HlslShaderConstants.h"
#include "../../../Model/Model.h"
#include "../../../Model/ModelPose.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	std::vector<Dx12Vertex> Dx12Instance::BuildVertices(const ModelGeometryData& geometryData, const bool useUpdateData) {
		const auto& positions = useUpdateData && geometryData.updatePositions.size() == geometryData.positions.size()
			? geometryData.updatePositions
			: geometryData.positions;
		const auto& normals = useUpdateData && geometryData.updateNormals.size() == geometryData.normals.size()
			? geometryData.updateNormals
			: geometryData.normals;
		const auto& uvs = useUpdateData && geometryData.updateUVs.size() == geometryData.uvs.size()
			? geometryData.updateUVs
			: geometryData.uvs;
		std::vector<Dx12Vertex> vertices(positions.size());
		for (size_t index = 0; index < positions.size(); index++) {
			auto& [position, normal, uv] = vertices[index];
			position = positions[index];
			if (index < normals.size())
				normal = normals[index];
			if (index < uvs.size())
				uv = uvs[index];
		}
		return vertices;
	}

	bool Dx12Instance::BuildIndexData(const ModelGeometryData& geometryData, std::vector<char>& indices, DXGI_FORMAT& format) {
		indices.clear();
		if (geometryData.indexElementSize == 1) {
			std::vector<uint16_t> convertedIndices(geometryData.indexCount);
			for (size_t index = 0; index < geometryData.indexCount; index++)
				convertedIndices[index] = geometryData.indices[index];
			const size_t byteSize = sizeof(uint16_t) * convertedIndices.size();
			const auto* bytes = reinterpret_cast<const char*>(convertedIndices.data());
			indices.assign(bytes, bytes + byteSize);
			format = DXGI_FORMAT_R16_UINT;
			return true;
		}
		if (geometryData.indexElementSize == 2) {
			indices = geometryData.indices;
			format = DXGI_FORMAT_R16_UINT;
			return true;
		}
		if (geometryData.indexElementSize == 4) {
			indices = geometryData.indices;
			format = DXGI_FORMAT_R32_UINT;
			return true;
		}
		return false;
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
		info.vertexBuffer.Destroy();
		info.indexBuffer.Destroy();
		info.modelVertexConstantBuffer.Destroy();
		for (Dx12Buffer& pixelConstantBuffer : info.modelPixelConstantBuffers)
			pixelConstantBuffer.Destroy();
		info.modelPixelConstantBuffers.clear();
		info.edgeVertexConstantBuffer.Destroy();
		for (Dx12Buffer& edgeSizeConstantBuffer : info.edgeSizeConstantBuffers)
			edgeSizeConstantBuffer.Destroy();
		info.edgeSizeConstantBuffers.clear();
		for (Dx12Buffer& edgePixelConstantBuffer : info.edgePixelConstantBuffers)
			edgePixelConstantBuffer.Destroy();
		info.edgePixelConstantBuffers.clear();
		info.groundShadowVertexConstantBuffer.Destroy();
		info.groundShadowPixelConstantBuffer.Destroy();
		info.textureDescriptorHeap.Reset();
		info.textureDescriptorSize = 0;
		info.vertexBufferView = {};
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
		const auto& geometryData = info.model->geometryData;
		std::vector<Dx12Vertex> vertices = BuildVertices(geometryData, false);
		std::vector<char> indices;
		DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
		if (vertices.empty() || !BuildIndexData(geometryData, indices, indexFormat) || indices.empty()) {
			std::cerr << "Failed to create DX12 model buffers: model has no geometry data.\n";
			return false;
		}
		const size_t vertexByteSize = sizeof(Dx12Vertex) * vertices.size();
		if (vertexByteSize > (std::numeric_limits<UINT>::max)() || indices.size() > (std::numeric_limits<UINT>::max)())
			return false;
		const auto& viewerInfo = info.viewer->GetDx12Info();
		if (!viewerInfo.deviceInfo)
			return false;
		const auto& deviceInfo = *viewerInfo.deviceInfo;
		if (!info.vertexBuffer.InitializeUpload(deviceInfo, vertexByteSize) ||
			!info.vertexBuffer.Write(vertices.data(), vertexByteSize))
			return false;
		if (!info.indexBuffer.InitializeUpload(deviceInfo, indices.size()) ||
			!info.indexBuffer.Write(indices.data(), indices.size()))
			return false;
		info.vertexBufferView.BufferLocation = info.vertexBuffer.ResolveGpuAddress();
		info.vertexBufferView.SizeInBytes = vertexByteSize;
		info.vertexBufferView.StrideInBytes = sizeof(Dx12Vertex);
		info.indexBufferView.BufferLocation = info.indexBuffer.ResolveGpuAddress();
		info.indexBufferView.SizeInBytes = indices.size();
		info.indexBufferView.Format = indexFormat;
		info.indexCount = geometryData.indexCount;
		const size_t vertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslModelVertexConstants));
		const size_t pixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslModelPixelConstants));
		if (!info.modelVertexConstantBuffer.InitializeUpload(deviceInfo, vertexConstantSize))
			return false;
		const size_t edgeVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslEdgeVertexConstants));
		const size_t edgeSizeConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslEdgeSizeConstants));
		const size_t edgePixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslEdgePixelConstants));
		const size_t groundShadowVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslGroundShadowVertexConstants));
		const size_t groundShadowPixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(HlslGroundShadowPixelConstants));
		if (!info.edgeVertexConstantBuffer.InitializeUpload(deviceInfo, edgeVertexConstantSize))
			return false;
		if (!info.groundShadowVertexConstantBuffer.InitializeUpload(deviceInfo, groundShadowVertexConstantSize))
			return false;
		if (!info.groundShadowPixelConstantBuffer.InitializeUpload(deviceInfo, groundShadowPixelConstantSize))
			return false;
		info.materials.reserve(info.model->materialData.materials.size());
		info.modelPixelConstantBuffers.resize(info.model->materialData.materials.size());
		for (Dx12Buffer& pixelConstantBuffer : info.modelPixelConstantBuffers) {
			if (!pixelConstantBuffer.InitializeUpload(deviceInfo, pixelConstantSize))
				return false;
		}
		info.edgeSizeConstantBuffers.resize(info.model->materialData.materials.size());
		for (Dx12Buffer& edgeSizeConstantBuffer : info.edgeSizeConstantBuffers) {
			if (!edgeSizeConstantBuffer.InitializeUpload(deviceInfo, edgeSizeConstantSize))
				return false;
		}
		info.edgePixelConstantBuffers.resize(info.model->materialData.materials.size());
		for (Dx12Buffer& edgePixelConstantBuffer : info.edgePixelConstantBuffers) {
			if (!edgePixelConstantBuffer.InitializeUpload(deviceInfo, edgePixelConstantSize))
				return false;
		}
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
		return CreateTextureDescriptors(info);
	}

	void Dx12Instance::Update() const {
		const auto& info = static_cast<const Dx12InstanceInfo&>(GetInfo());
		if (info.model == nullptr || !info.vertexBuffer.IsInitialized())
			return;
		const ModelPose pose(*info.model);
		pose.Update();
		const std::vector<Dx12Vertex> vertices = BuildVertices(info.model->geometryData, true);
		if (vertices.empty())
			return;
		if (!info.vertexBuffer.Write(vertices.data(), sizeof(Dx12Vertex) * vertices.size()))
			std::cerr << "Failed to update DX12 vertex buffer.\n";
	}
}
