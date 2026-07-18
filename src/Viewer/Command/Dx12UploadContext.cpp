#include "Viewer/Command/Dx12UploadContext.h"

#include "Viewer/Synchronization/Dx12Barrier.h"

namespace Chrivent {
	void Dx12UploadContext::Reset() {
		if (fenceEvent != nullptr) {
			CloseHandle(fenceEvent);
			fenceEvent = nullptr;
		}
		fence.Reset();
		enhancedCommandList.Reset();
		commandList.Reset();
		commandAllocator.Reset();
		fenceValue = 0;
	}

	GraphicsResult<void> Dx12UploadContext::Begin(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetDevice() || !sourceDevice.GetCommandQueue()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "업로드 command 기록 시작",
				"DirectX 12 device 또는 command queue를 사용할 수 없습니다"));
		}
		if (!commandAllocator) {
			HRESULT result = sourceDevice.GetDevice()->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
			if (FAILED(result)) {
				Reset();
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "업로드 command allocator 생성",
					"DirectX 12 업로드 command allocator를 만들지 못했습니다", result, true));
			}
			result = sourceDevice.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
			if (FAILED(result)) {
				Reset();
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "업로드 command list 생성",
					"DirectX 12 업로드 command list를 만들지 못했습니다", result, true));
			}
			if (sourceDevice.SupportsEnhancedBarriers()) {
				result = commandList.As(&enhancedCommandList);
				if (FAILED(result)) {
					Reset();
					return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
						GraphicsErrorCode::UnsupportedFeature, "업로드 향상된 command list 조회",
						"DirectX 12 향상된 업로드 command list를 가져오지 못했습니다", result, true));
				}
			}
			result = sourceDevice.GetDevice()->CreateFence(
				0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
			if (FAILED(result)) {
				Reset();
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "업로드 fence 생성",
					"DirectX 12 업로드 fence를 만들지 못했습니다", result, true));
			}
			fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr) {
				const DWORD error = GetLastError();
				Reset();
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "업로드 fence event 생성",
					"DirectX 12 업로드 fence event를 만들지 못했습니다", error, true));
			}
			return {};
		}
		if (!commandList || !fence || !fenceEvent) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "업로드 command context 재사용",
				"DirectX 12 업로드 command context가 부분 초기화 상태입니다"));
		}
		HRESULT result = commandAllocator->Reset();
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "업로드 command allocator 초기화",
				"DirectX 12 업로드 command allocator를 초기화하지 못했습니다", result, true));
		}
		result = commandList->Reset(commandAllocator.Get(), nullptr);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "업로드 command list 초기화",
				"DirectX 12 업로드 command list를 초기화하지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> Dx12UploadContext::SubmitAndWait(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetCommandQueue() || !commandList || !fence || !fenceEvent) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "업로드 command 제출",
				"DirectX 12 업로드 command context를 사용할 수 없습니다"));
		}
		HRESULT result = commandList->Close();
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "업로드 command list 종료",
				"DirectX 12 업로드 command list 기록을 끝내지 못했습니다", result, true));
		}
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		sourceDevice.GetCommandQueue()->ExecuteCommandLists(1, commandLists);
		fenceValue++;
		result = sourceDevice.GetCommandQueue()->Signal(fence.Get(), fenceValue);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandSubmissionFailed, "업로드 fence signal",
				"DirectX 12 업로드 fence 값을 기록하지 못했습니다", result, true));
		}
		if (fence->GetCompletedValue() >= fenceValue)
			return {};
		result = fence->SetEventOnCompletion(fenceValue, fenceEvent);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::SynchronizationFailed, "업로드 fence event 설정",
				"DirectX 12 업로드 fence 완료 event를 설정하지 못했습니다", result, true));
		}
		const DWORD waitResult = WaitForSingleObject(fenceEvent, INFINITE);
		if (waitResult != WAIT_OBJECT_0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::SynchronizationFailed, "업로드 fence 대기",
				"DirectX 12 업로드 fence를 기다리지 못했습니다", waitResult, true));
		}
		return {};
	}

	Dx12UploadContext::~Dx12UploadContext() {
		Reset();
	}

	GraphicsResult<void> Dx12UploadContext::UploadTexture(const Dx12Device& sourceDevice,
		ID3D12Resource* destination, ID3D12Resource* source,
		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout) {
		if (destination == nullptr || source == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "texture 업로드",
				"복사할 DirectX 12 texture 또는 upload buffer가 없습니다"));
		}
		const auto beginResult = Begin(sourceDevice);
		if (!beginResult)
			return std::unexpected(beginResult.error());
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

	GraphicsResult<void> Dx12UploadContext::UploadIndexBuffer(const Dx12Device& sourceDevice,
		ID3D12Resource* destination, ID3D12Resource* source, const UINT64 size) {
		if (destination == nullptr || source == nullptr || size == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "index buffer 업로드",
				"복사할 DirectX 12 index buffer 또는 크기가 올바르지 않습니다"));
		}
		const auto beginResult = Begin(sourceDevice);
		if (!beginResult)
			return std::unexpected(beginResult.error());
		commandList->CopyBufferRegion(destination, 0, source, 0, size);
		Dx12Barrier::TransitionBuffer(commandList.Get(), enhancedCommandList.Get(), destination,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		return SubmitAndWait(sourceDevice);
	}
}
