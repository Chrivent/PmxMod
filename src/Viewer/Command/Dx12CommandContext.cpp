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

	GraphicsError::Result<void> Dx12CommandContext::Initialize(const Dx12Device& sourceDevice) {
		Reset();
		if (!sourceDevice.GetDevice() || !sourceDevice.GetCommandQueue()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "command context 초기화",
				"DirectX 12 device 또는 command queue를 사용할 수 없습니다"));
		}
		for (auto& commandAllocator : commandAllocators) {
			const HRESULT result = sourceDevice.GetDevice()->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
			if (FAILED(result)) {
				Reset();
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "command allocator 생성",
					"DirectX 12 command allocator를 만들지 못했습니다", result, true));
			}
		}
		HRESULT result = sourceDevice.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList));
		if (FAILED(result)) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "command list 생성",
				"DirectX 12 command list를 만들지 못했습니다", result, true));
		}
		if (sourceDevice.SupportsEnhancedBarriers()) {
			result = commandList.As(&enhancedCommandList);
			if (FAILED(result)) {
				Reset();
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::UnsupportedFeature, "향상된 command list 조회",
					"DirectX 12 향상된 배리어 command list를 가져오지 못했습니다", result, true));
			}
		}
		result = commandList->Close();
		if (FAILED(result)) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "command list 초기 종료",
				"DirectX 12 command list를 초기 상태로 닫지 못했습니다", result, true));
		}
		result = sourceDevice.GetDevice()->CreateFence(
			0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		if (FAILED(result)) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "fence 생성",
				"DirectX 12 fence를 만들지 못했습니다", result, true));
		}
		fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent) {
			const DWORD error = GetLastError();
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "fence event 생성",
				"DirectX 12 fence event를 만들지 못했습니다", error, true));
		}
		nextFenceValue = 1;
		return {};
	}

	GraphicsError::Result<void> Dx12CommandContext::BeginFrame(const Dx12Device& sourceDevice,
		const UINT frameIndex) {
		if (!sourceDevice.GetDevice() || !commandList || !fence || !fenceEvent) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "프레임 command 기록 시작",
				"DirectX 12 command context가 초기화되지 않았습니다"));
		}
		this->frameIndex = frameIndex % FrameBuffering::dx12BufferCount;
		const uint64_t frameFenceValue = frameFenceValues[this->frameIndex];
		if (frameFenceValue != 0 && fence->GetCompletedValue() < frameFenceValue) {
			const HRESULT result = fence->SetEventOnCompletion(frameFenceValue, fenceEvent);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::SynchronizationFailed, "프레임 fence event 설정",
					"DirectX 12 프레임 fence 완료 event를 설정하지 못했습니다", result, true));
			}
			const DWORD waitResult = WaitForSingleObject(fenceEvent, INFINITE);
			if (waitResult != WAIT_OBJECT_0) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::SynchronizationFailed, "프레임 fence 대기",
					"DirectX 12 프레임 fence를 기다리지 못했습니다", waitResult, true));
			}
		}
		ID3D12CommandAllocator* commandAllocator = commandAllocators[this->frameIndex].Get();
		if (commandAllocator == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "command allocator 초기화",
				"현재 프레임의 DirectX 12 command allocator가 없습니다"));
		}
		HRESULT result = commandAllocator->Reset();
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "command allocator 초기화",
				"DirectX 12 command allocator를 초기화하지 못했습니다", result, true));
		}
		result = commandList->Reset(commandAllocator, nullptr);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "command list 초기화",
				"DirectX 12 command list를 초기화하지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> Dx12CommandContext::Execute(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetCommandQueue() || !commandList || !fence) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "command list 제출",
				"DirectX 12 command queue 또는 command context를 사용할 수 없습니다"));
		}
		HRESULT result = commandList->Close();
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "command list 종료",
				"DirectX 12 command list 기록을 끝내지 못했습니다", result, true));
		}
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		sourceDevice.GetCommandQueue()->ExecuteCommandLists(1, commandLists);
		const uint64_t signalValue = nextFenceValue;
		result = sourceDevice.GetCommandQueue()->Signal(fence.Get(), signalValue);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandSubmissionFailed, "프레임 fence signal",
				"DirectX 12 command queue에 fence 값을 기록하지 못했습니다", result, true));
		}
		frameFenceValues[frameIndex] = signalValue;
		nextFenceValue++;
		return {};
	}

	GraphicsError::Result<void> Dx12CommandContext::WaitForGpu(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetCommandQueue() || !fence || !fenceEvent) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "GPU 대기",
				"DirectX 12 command queue 또는 fence를 사용할 수 없습니다"));
		}
		const uint64_t waitValue = nextFenceValue;
		HRESULT result = sourceDevice.GetCommandQueue()->Signal(fence.Get(), waitValue);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::SynchronizationFailed, "GPU 대기 fence signal",
				"DirectX 12 GPU 대기 fence 값을 기록하지 못했습니다", result, true));
		}
		nextFenceValue++;
		if (fence->GetCompletedValue() >= waitValue)
			return {};
		result = fence->SetEventOnCompletion(waitValue, fenceEvent);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::SynchronizationFailed, "GPU 대기 event 설정",
				"DirectX 12 GPU 대기 event를 설정하지 못했습니다", result, true));
		}
		const DWORD waitResult = WaitForSingleObject(fenceEvent, INFINITE);
		if (waitResult != WAIT_OBJECT_0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::SynchronizationFailed, "GPU fence 대기",
				"DirectX 12 GPU fence를 기다리지 못했습니다", waitResult, true));
		}
		for (uint64_t& frameFenceValue : frameFenceValues)
			frameFenceValue = 0;
		return {};
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
