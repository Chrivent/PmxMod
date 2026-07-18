#include "Viewer/Command/Dx12CommandContext.h"

namespace Chrivent {
	void Dx12CommandContext::ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList,
		const int width, const int height) {
		if (commandList == nullptr)
			return;
		D3D12_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);
		D3D12_RECT scissor{};
		scissor.right = width;
		scissor.bottom = height;
		commandList->RSSetScissorRects(1, &scissor);
	}

	bool Dx12CommandContext::Initialize(const Dx12Device& sourceDevice) {
		Reset();
		if (!sourceDevice.GetDevice() || !sourceDevice.GetCommandQueue())
			return false;
		for (auto& commandAllocator : commandAllocators) {
			if (FAILED(sourceDevice.GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
				return false;
		}
		if (FAILED(sourceDevice.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList))))
			return false;
		if (sourceDevice.SupportsEnhancedBarriers() && FAILED(commandList.As(&enhancedCommandList)))
			return false;
		if (FAILED(commandList->Close()))
			return false;
		if (FAILED(sourceDevice.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
			return false;
		fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent)
			return false;
		nextFenceValue = 1;
		return true;
	}

	bool Dx12CommandContext::BeginFrame(const Dx12Device& sourceDevice, const UINT frameIndex) {
		if (!sourceDevice.GetDevice() || !commandList || !fence || !fenceEvent)
			return false;
		this->frameIndex = frameIndex % FrameBuffering::dx12BufferCount;
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
		if (!sourceDevice.GetCommandQueue() || !commandList || !fence)
			return false;
		if (FAILED(commandList->Close()))
			return false;
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		sourceDevice.GetCommandQueue()->ExecuteCommandLists(1, commandLists);
		const uint64_t signalValue = nextFenceValue;
		if (FAILED(sourceDevice.GetCommandQueue()->Signal(fence.Get(), signalValue)))
			return false;
		frameFenceValues[frameIndex] = signalValue;
		nextFenceValue++;
		return true;
	}

	bool Dx12CommandContext::WaitForGpu(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetCommandQueue() || !fence || !fenceEvent)
			return false;
		const uint64_t waitValue = nextFenceValue;
		if (FAILED(sourceDevice.GetCommandQueue()->Signal(fence.Get(), waitValue)))
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

	void Dx12CommandContext::Reset() {
		if (fenceEvent) {
			CloseHandle(fenceEvent);
			fenceEvent = nullptr;
		}
		fence.Reset();
		enhancedCommandList.Reset();
		commandList.Reset();
		for (auto& commandAllocator : commandAllocators)
			commandAllocator.Reset();
		for (uint64_t& frameFenceValue : frameFenceValues)
			frameFenceValue = 0;
		nextFenceValue = 1;
		frameIndex = 0;
	}
}
