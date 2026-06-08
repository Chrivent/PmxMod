#include "Dx12CommandContext.h"

namespace Chrivent {
	bool Dx12CommandContext::Initialize(const Dx12DeviceInfo& deviceInfo) {
		Destroy();
		if (!deviceInfo.device || !deviceInfo.commandQueue)
			return false;
		for (auto& commandAllocator : info.commandAllocators) {
			if (FAILED(deviceInfo.device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&commandAllocator))))
				return false;
		}
		if (FAILED(deviceInfo.device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			info.commandAllocators[0].Get(),
			nullptr,
			IID_PPV_ARGS(&info.commandList))))
			return false;
		if (FAILED(info.commandList->Close()))
			return false;
		if (FAILED(deviceInfo.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&info.fence))))
			return false;
		info.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!info.fenceEvent)
			return false;
		info.nextFenceValue = 1;
		return true;
	}

	bool Dx12CommandContext::BeginFrame(const Dx12DeviceInfo& deviceInfo, const UINT frameIndex) {
		if (!deviceInfo.device || !info.commandList || !info.fence || !info.fenceEvent)
			return false;
		info.frameIndex = frameIndex % Dx12CommandContextInfo::kFrameCount;
		const uint64_t frameFenceValue = info.frameFenceValues[info.frameIndex];
		if (frameFenceValue != 0 && info.fence->GetCompletedValue() < frameFenceValue) {
			if (FAILED(info.fence->SetEventOnCompletion(frameFenceValue, info.fenceEvent)))
				return false;
			WaitForSingleObject(info.fenceEvent, INFINITE);
		}
		ID3D12CommandAllocator* commandAllocator = info.commandAllocators[info.frameIndex].Get();
		if (commandAllocator == nullptr)
			return false;
		if (FAILED(commandAllocator->Reset()))
			return false;
		return SUCCEEDED(info.commandList->Reset(commandAllocator, nullptr));
	}

	bool Dx12CommandContext::Execute(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.commandQueue || !info.commandList || !info.fence)
			return false;
		if (FAILED(info.commandList->Close()))
			return false;
		ID3D12CommandList* commandLists[] = { info.commandList.Get() };
		deviceInfo.commandQueue->ExecuteCommandLists(1, commandLists);
		const uint64_t signalValue = info.nextFenceValue;
		if (FAILED(deviceInfo.commandQueue->Signal(info.fence.Get(), signalValue)))
			return false;
		info.frameFenceValues[info.frameIndex] = signalValue;
		info.nextFenceValue++;
		return true;
	}

	bool Dx12CommandContext::WaitForGpu(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.commandQueue || !info.fence || !info.fenceEvent)
			return false;
		const uint64_t waitValue = info.nextFenceValue;
		if (FAILED(deviceInfo.commandQueue->Signal(info.fence.Get(), waitValue)))
			return false;
		info.nextFenceValue++;
		if (info.fence->GetCompletedValue() >= waitValue)
			return true;
		if (FAILED(info.fence->SetEventOnCompletion(waitValue, info.fenceEvent)))
			return false;
		WaitForSingleObject(info.fenceEvent, INFINITE);
		info.frameFenceValues.fill(0);
		return true;
	}

	void Dx12CommandContext::Destroy() {
		if (info.fenceEvent) {
			CloseHandle(info.fenceEvent);
			info.fenceEvent = nullptr;
		}
		info.fence.Reset();
		info.commandList.Reset();
		for (auto& commandAllocator : info.commandAllocators)
			commandAllocator.Reset();
		info.frameFenceValues = {};
		info.nextFenceValue = 1;
		info.frameIndex = 0;
	}
}
