#pragma once

#include "Dx12Device.h"

#include <cstdint>
#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

namespace Chrivent {
	struct Dx12CommandContextInfo {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		HANDLE fenceEvent = nullptr;
		uint64_t fenceValue = 0;
	};

	class Dx12CommandContext {
		Dx12CommandContextInfo info;

	public:
		// DX12 명령 큐와 프레임 명령 기록 리소스를 초기화한다.
		bool Initialize(const Dx12DeviceInfo& deviceInfo);
		// CPU와 GPU를 동기화해 기록된 명령이 모두 끝날 때까지 대기한다.
		bool WaitForGpu(const Dx12DeviceInfo& deviceInfo);
		// 생성한 DX12 명령 리소스를 해제한다.
		void Destroy();

		const Dx12CommandContextInfo& GetInfo() const { return info; }
	};
}
