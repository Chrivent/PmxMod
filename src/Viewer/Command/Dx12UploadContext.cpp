#include "Viewer/Command/Dx12UploadContext.h"

#include "Viewer/Synchronization/Dx12Barrier.h"

namespace Chrivent {
	bool Dx12UploadContext::Begin(const Dx12Device& sourceDevice) {
		if (!sourceDevice.device || !sourceDevice.commandQueue)
			return false;
		if (!commandAllocator) {
			if (FAILED(sourceDevice.device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
				return false;
			if (FAILED(sourceDevice.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList))))
				return false;
			if (sourceDevice.capabilities.supportsEnhancedBarriers
				&& FAILED(commandList.As(&enhancedCommandList)))
				return false;
			if (FAILED(sourceDevice.device->CreateFence(
				0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
				return false;
			fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
			return fenceEvent != nullptr;
		}
		return SUCCEEDED(commandAllocator->Reset())
			&& SUCCEEDED(commandList->Reset(commandAllocator.Get(), nullptr));
	}

	bool Dx12UploadContext::SubmitAndWait(const Dx12Device& sourceDevice) {
		if (FAILED(commandList->Close()))
			return false;
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		sourceDevice.commandQueue->ExecuteCommandLists(1, commandLists);
		fenceValue++;
		if (FAILED(sourceDevice.commandQueue->Signal(fence.Get(), fenceValue)))
			return false;
		if (fence->GetCompletedValue() >= fenceValue)
			return true;
		if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent)))
			return false;
		return WaitForSingleObject(fenceEvent, INFINITE) == WAIT_OBJECT_0;
	}

	Dx12UploadContext::~Dx12UploadContext() {
		if (fenceEvent != nullptr)
			CloseHandle(fenceEvent);
	}

	bool Dx12UploadContext::UploadTexture(const Dx12Device& sourceDevice,
		ID3D12Resource* destination, ID3D12Resource* source,
		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout) {
		if (destination == nullptr || source == nullptr || !Begin(sourceDevice))
			return false;
		D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
		destinationLocation.pResource = destination;
		destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		D3D12_TEXTURE_COPY_LOCATION sourceLocation;
		sourceLocation.pResource = source;
		sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		sourceLocation.PlacedFootprint = layout;
		commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
		Dx12Barrier::Transition(commandList.Get(), enhancedCommandList.Get(), destination,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return SubmitAndWait(sourceDevice);
	}

	bool Dx12UploadContext::UploadIndexBuffer(const Dx12Device& sourceDevice,
		ID3D12Resource* destination, ID3D12Resource* source, const UINT64 size) {
		if (destination == nullptr || source == nullptr || size == 0 || !Begin(sourceDevice))
			return false;
		commandList->CopyBufferRegion(destination, 0, source, 0, size);
		Dx12Barrier::TransitionBuffer(commandList.Get(), enhancedCommandList.Get(), destination,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		return SubmitAndWait(sourceDevice);
	}
}
