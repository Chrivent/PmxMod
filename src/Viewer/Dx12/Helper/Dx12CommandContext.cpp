#include "Dx12CommandContext.h"

namespace Chrivent {
	bool Dx12CommandContext::Initialize(const Dx12DeviceInfo& deviceInfo) {
		Destroy();
		if (!deviceInfo.device || !deviceInfo.commandQueue)
			return false;
		if (FAILED(deviceInfo.device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&info.commandAllocator))))
			return false;
		if (FAILED(deviceInfo.device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			info.commandAllocator.Get(),
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
		info.fenceValue = 1;
		return true;
	}

	bool Dx12CommandContext::BeginFrame() const {
		if (!info.commandAllocator || !info.commandList)
			return false;
		if (FAILED(info.commandAllocator->Reset()))
			return false;
		return SUCCEEDED(info.commandList->Reset(info.commandAllocator.Get(), nullptr));
	}

	bool Dx12CommandContext::Execute(const Dx12DeviceInfo& deviceInfo) const {
		if (!deviceInfo.commandQueue || !info.commandList)
			return false;
		if (FAILED(info.commandList->Close()))
			return false;
		ID3D12CommandList* commandLists[] = { info.commandList.Get() };
		deviceInfo.commandQueue->ExecuteCommandLists(1, commandLists);
		return true;
	}

	bool Dx12CommandContext::WaitForGpu(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.commandQueue || !info.fence || !info.fenceEvent)
			return false;
		const uint64_t waitValue = info.fenceValue;
		if (FAILED(deviceInfo.commandQueue->Signal(info.fence.Get(), waitValue)))
			return false;
		info.fenceValue++;
		if (info.fence->GetCompletedValue() >= waitValue)
			return true;
		if (FAILED(info.fence->SetEventOnCompletion(waitValue, info.fenceEvent)))
			return false;
		WaitForSingleObject(info.fenceEvent, INFINITE);
		return true;
	}

	void Dx12CommandContext::Destroy() {
		if (info.fenceEvent) {
			CloseHandle(info.fenceEvent);
			info.fenceEvent = nullptr;
		}
		info.fence.Reset();
		info.commandList.Reset();
		info.commandAllocator.Reset();
		info.fenceValue = 0;
	}
}
