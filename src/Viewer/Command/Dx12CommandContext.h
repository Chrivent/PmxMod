#pragma once

#include "Viewer/Device/Dx12Device.h"
#include "Viewer/Synchronization/FrameBuffering.h"

#include <cstdint>
#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 명령 할당자와 그래픽 명령 목록의 기록 상태를 관리한다.
	class Dx12CommandContext {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameBuffering::dx12BufferCount]{};
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> enhancedCommandList;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		HANDLE fenceEvent = nullptr;
		uint64_t frameFenceValues[FrameBuffering::dx12BufferCount]{};
		uint64_t nextFenceValue = 1;
		UINT frameIndex = 0;

		// 생성한 DX12 명령 리소스를 해제한다.
		void Reset();

	public:
		Dx12CommandContext() = default;
		~Dx12CommandContext() { Reset(); }

		Dx12CommandContext(const Dx12CommandContext&) = delete;
		Dx12CommandContext& operator=(const Dx12CommandContext&) = delete;

		ID3D12GraphicsCommandList* TryGetCommandList() const { return commandList.Get(); }
		ID3D12GraphicsCommandList7* TryGetEnhancedCommandList() const { return enhancedCommandList.Get(); }

		// 현재 출력 크기에 맞는 viewport와 scissor rect를 명령 목록에 적용한다.
		static void ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList, int width, int height);
		// DX12 명령 큐와 프레임 명령 기록 리소스를 초기화한다.
		GraphicsError::Result<void> Initialize(const Dx12Device& sourceDevice);
		// 한 프레임의 명령 기록을 시작할 수 있도록 allocator와 list를 초기화한다.
		GraphicsError::Result<void> BeginFrame(const Dx12Device& sourceDevice, UINT frameIndex);
		// 기록한 명령 리스트를 닫고 command queue에 제출한다.
		GraphicsError::Result<void> Execute(const Dx12Device& sourceDevice);
		// CPU와 GPU를 동기화해 기록된 명령이 모두 끝날 때까지 대기한다.
		GraphicsError::Result<void> WaitForGpu(const Dx12Device& sourceDevice);
	};
}
