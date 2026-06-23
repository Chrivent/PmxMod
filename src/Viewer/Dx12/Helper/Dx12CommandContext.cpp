#include "Viewer/Dx12/Helper/Dx12CommandContext.h"

namespace Chrivent {
	bool Dx12CommandContext::Initialize(const Dx12Device& sourceDevice) {
		Destroy();
		if (!sourceDevice.device || !sourceDevice.commandQueue)
			return false;
		for (auto& commandAllocator : commandAllocators) {
			if (FAILED(sourceDevice.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
				return false;
		}
		if (FAILED(sourceDevice.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList))))
			return false;
		if (FAILED(commandList->Close()))
			return false;
		if (FAILED(sourceDevice.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
			return false;
		fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent)
			return false;
		nextFenceValue = 1;
		return true;
	}

	bool Dx12CommandContext::BeginFrame(const Dx12Device& sourceDevice, const UINT frameIndex) {
		if (!sourceDevice.device || !commandList || !fence || !fenceEvent)
			return false;
		this->frameIndex = frameIndex % kFrameCount;
		const uint64_t frameFenceValue = frameFenceValues[this->frameIndex];
		if (frameFenceValue != 0 && fence->GetCompletedValue() < frameFenceValue) {
			if (FAILED(fence->SetEventOnCompletion(frameFenceValue, fenceEvent)))
				return false;
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		ID3D12CommandAllocator* commandAllocator = commandAllocators[this->frameIndex].Get();
		if (commandAllocator == nullptr)
			return false;
		if (FAILED(commandAllocator->Reset()))
			return false;
		return SUCCEEDED(commandList->Reset(commandAllocator, nullptr));
	}

	bool Dx12CommandContext::Execute(const Dx12Device& sourceDevice) {
		if (!sourceDevice.commandQueue || !commandList || !fence)
			return false;
		if (FAILED(commandList->Close()))
			return false;
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		sourceDevice.commandQueue->ExecuteCommandLists(1, commandLists);
		const uint64_t signalValue = nextFenceValue;
		if (FAILED(sourceDevice.commandQueue->Signal(fence.Get(), signalValue)))
			return false;
		frameFenceValues[frameIndex] = signalValue;
		nextFenceValue++;
		return true;
	}

	bool Dx12CommandContext::WaitForGpu(const Dx12Device& sourceDevice) {
		if (!sourceDevice.commandQueue || !fence || !fenceEvent)
			return false;
		const uint64_t waitValue = nextFenceValue;
		if (FAILED(sourceDevice.commandQueue->Signal(fence.Get(), waitValue)))
			return false;
		nextFenceValue++;
		if (fence->GetCompletedValue() >= waitValue)
			return true;
		if (FAILED(fence->SetEventOnCompletion(waitValue, fenceEvent)))
			return false;
		WaitForSingleObject(fenceEvent, INFINITE);
		for (uint64_t& frameFenceValue : frameFenceValues)
			frameFenceValue = 0;
		return true;
	}

	void Dx12CommandContext::Destroy() {
		if (fenceEvent) {
			CloseHandle(fenceEvent);
			fenceEvent = nullptr;
		}
		fence.Reset();
		commandList.Reset();
		for (auto& commandAllocator : commandAllocators)
			commandAllocator.Reset();
		for (uint64_t& frameFenceValue : frameFenceValues)
			frameFenceValue = 0;
		nextFenceValue = 1;
		frameIndex = 0;
	}
}
