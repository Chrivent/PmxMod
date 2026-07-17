#include "Viewer/Instance/Dx12Instance.h"

#include "Viewer/Drawer/Dx12Drawer.h"
#include "Viewer/DrawContext/Dx12DrawContext.h"
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
		if (!modelResources.indexBuffer.InitializeUpload(device, indexData.bytes.size()) ||
			!modelResources.indexBuffer.Write(std::as_bytes(std::span(indexData.bytes))))
			return false;
		modelResources.indexBufferView.BufferLocation = modelResources.indexBuffer.GetGpuAddress();
		modelResources.indexBufferView.SizeInBytes = indexData.bytes.size();
		modelResources.indexBufferView.Format = indexFormat;
		modelResources.indexCount = indexData.indexCount;
		return true;
	}

	bool Dx12Instance::CreateConstantBuffers() {
		const size_t modelVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelVertexConstants));
		const size_t postProcessVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(
			std::max(sizeof(ModelVertexConstants), sizeof(SceneVelocityVertexConstants)));
		const size_t vertexConstantSize = modelVertexConstantSize + postProcessVertexConstantSize;
		const size_t pixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(ModelPixelConstants));
		const size_t sceneSurfaceConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(SceneSurfacePixelConstants));
		const size_t materialPixelBufferSize = pixelConstantSize + sceneSurfaceConstantSize;
		const size_t edgeVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(EdgeVertexConstants));
		const size_t edgePixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(EdgePixelConstants));
		const size_t groundShadowVertexConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(GroundShadowVertexConstants));
		const size_t groundShadowPixelConstantSize = Dx12Buffer::AlignConstantBufferSize(sizeof(GroundShadowPixelConstants));
		for (size_t frameIndex = 0; frameIndex < FrameBuffering::dx12BufferCount; frameIndex++) {
			if (!modelResources.modelVertexConstantBuffers[frameIndex].InitializeUpload(device, vertexConstantSize) ||
				!modelResources.groundShadowVertexConstantBuffers[frameIndex].InitializeUpload(
					device, groundShadowVertexConstantSize) ||
				!modelResources.groundShadowPixelConstantBuffers[frameIndex].InitializeUpload(
					device, groundShadowPixelConstantSize))
				return false;
		}
		const size_t materialCount = model->materialData.materials.size();
		modelResources.modelPixelConstantBuffers.resize(materialCount);
		for (auto& buffers : modelResources.modelPixelConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(FrameBuffering::dx12BufferCount);
			for (size_t i = 0; i < FrameBuffering::dx12BufferCount; i++) {
				if (!buffers[i].InitializeUpload(device, materialPixelBufferSize))
					return false;
			}
		}
		modelResources.edgeVertexConstantBuffers.resize(materialCount);
		for (auto& buffers : modelResources.edgeVertexConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(FrameBuffering::dx12BufferCount);
			for (size_t i = 0; i < FrameBuffering::dx12BufferCount; i++) {
				if (!buffers[i].InitializeUpload(device, edgeVertexConstantSize))
					return false;
			}
		}
		modelResources.edgePixelConstantBuffers.resize(materialCount);
		for (auto& buffers : modelResources.edgePixelConstantBuffers) {
			buffers = std::make_unique<Dx12Buffer[]>(FrameBuffering::dx12BufferCount);
			for (size_t i = 0; i < FrameBuffering::dx12BufferCount; i++) {
				if (!buffers[i].InitializeUpload(device, edgePixelConstantSize))
					return false;
			}
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
		Dx12TextureCache& sourceTextureCache, const Dx12Texture& sourceDummyTexture,
		const Dx12DrawContext& sourceDrawContext)
		: device(sourceDevice), textureCache(sourceTextureCache), dummyTexture(sourceDummyTexture),
		drawContext(sourceDrawContext) {
		drawer = std::make_unique<Dx12Drawer>(*this, modelResources, drawContext, sourceViewer);
	}

	void Dx12Instance::ResetRendererResources() {
		for (Dx12Buffer& vertexBuffer : modelResources.vertexBuffers)
			vertexBuffer.Reset();
		modelResources.indexBuffer.Reset();
		for (Dx12Buffer& buffer : modelResources.modelVertexConstantBuffers)
			buffer.Reset();
		for (auto& buffers : modelResources.modelPixelConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < FrameBuffering::dx12BufferCount; i++)
				buffers[i].Reset();
		}
		modelResources.modelPixelConstantBuffers.clear();
		for (auto& buffers : modelResources.edgeVertexConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < FrameBuffering::dx12BufferCount; i++)
				buffers[i].Reset();
		}
		modelResources.edgeVertexConstantBuffers.clear();
		for (auto& buffers : modelResources.edgePixelConstantBuffers) {
			if (!buffers)
				continue;
			for (size_t i = 0; i < FrameBuffering::dx12BufferCount; i++)
				buffers[i].Reset();
		}
		modelResources.edgePixelConstantBuffers.clear();
		for (Dx12Buffer& buffer : modelResources.groundShadowVertexConstantBuffers)
			buffer.Reset();
		for (Dx12Buffer& buffer : modelResources.groundShadowPixelConstantBuffers)
			buffer.Reset();
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
