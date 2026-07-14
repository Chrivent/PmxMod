#include "Viewer/Texture/Dx12TextureCache.h"

#include "Viewer/Synchronization/Dx12Barrier.h"

#include <windows.h>

namespace Chrivent {
	bool Dx12TextureCache::UploadRgbaPixels(const Dx12Device& sourceDevice, const unsigned char* pixels,
		const UINT width, const UINT height, Dx12Texture& texture) {
		if (!sourceDevice.device || !sourceDevice.commandQueue || pixels == nullptr || width == 0 || height == 0)
			return false;
		D3D12_RESOURCE_DESC textureDesc{};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		defaultHeap.CreationNodeMask = 1;
		defaultHeap.VisibleNodeMask = 1;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture.resource))))
			return false;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
		UINT rowCount = 0;
		UINT64 uploadByteSize = 0;
		sourceDevice.device->GetCopyableFootprints(&textureDesc, 0, 1, 0,
			&layout, &rowCount, nullptr, &uploadByteSize);
		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeap.CreationNodeMask = 1;
		uploadHeap.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC uploadDesc{};
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width = uploadByteSize;
		uploadDesc.Height = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.SampleDesc.Count = 1;
		uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&uploadBuffer))))
			return false;
		void* mappedData = nullptr;
		constexpr D3D12_RANGE readRange{ 0, 0 };
		if (FAILED(uploadBuffer->Map(0, &readRange, &mappedData)))
			return false;
		auto* destination = static_cast<std::uint8_t*>(mappedData) + layout.Offset;
		const UINT sourcePitch = width * 4;
		for (UINT row = 0; row < rowCount; row++)
			std::memcpy(destination + layout.Footprint.RowPitch * row, pixels + sourcePitch * row, sourcePitch);
		uploadBuffer->Unmap(0, nullptr);
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		if (FAILED(sourceDevice.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
			return false;
		if (FAILED(sourceDevice.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList))))
			return false;
		D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
		destinationLocation.pResource = texture.resource.Get();
		destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
		sourceLocation.pResource = uploadBuffer.Get();
		sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		sourceLocation.PlacedFootprint = layout;
		commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> enhancedCommandList;
		if (sourceDevice.capabilities.supportsEnhancedBarriers && FAILED(commandList.As(&enhancedCommandList)))
			return false;
		Dx12Barrier::Transition(commandList.Get(), enhancedCommandList.Get(), texture.resource.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (FAILED(commandList->Close()))
			return false;
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		sourceDevice.commandQueue->ExecuteCommandLists(1, commandLists);
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		if (FAILED(sourceDevice.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
			return false;
		HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
			return false;
		constexpr UINT64 fenceValue = 1;
		if (FAILED(sourceDevice.commandQueue->Signal(fence.Get(), fenceValue))) {
			CloseHandle(fenceEvent);
			return false;
		}
		if (fence->GetCompletedValue() < fenceValue) {
			if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
				CloseHandle(fenceEvent);
				return false;
			}
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		CloseHandle(fenceEvent);
		texture.width = width;
		texture.height = height;
		texture.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		return true;
	}

	Dx12Texture Dx12TextureCache::Load(const Dx12Device& sourceDevice, const std::filesystem::path& texturePath) {
		const TextureKey key{ TextureKind::File, texturePath };
		if (const auto texture = FindCachedTexture<Dx12Texture>(key))
			return *texture;
		const auto [pixels, width, height, components] = LoadImageRgba(texturePath);
		if (!pixels)
			return {};
		const auto texture = std::make_shared<Dx12Texture>();
		texture->key = key;
		texture->hasAlpha = components == 4;
		const bool uploaded = UploadRgbaPixels(
			sourceDevice, pixels.get(), width, height, *texture);
		if (!uploaded)
			return {};
		textures[key] = texture;
		return *texture;
	}

	Dx12Texture Dx12TextureCache::CreateWhiteTexture(const Dx12Device& sourceDevice) {
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture<Dx12Texture>(key))
			return *texture;
		constexpr unsigned char white[] = { 255, 255, 255, 255 };
		const auto texture = std::make_shared<Dx12Texture>();
		texture->key = key;
		texture->hasAlpha = false;
		if (!UploadRgbaPixels(sourceDevice, white, 1, 1, *texture))
			return {};
		textures[key] = texture;
		return *texture;
	}
}
