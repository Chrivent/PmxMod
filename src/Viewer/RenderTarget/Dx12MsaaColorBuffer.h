#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 장면 렌더링용 MSAA 색상 타깃과 view를 관리한다.
	class Dx12MsaaColorBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> renderTarget;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
		UINT sampleCount = 1;

	public:
		ID3D12Resource* GetResource() const { return renderTarget.Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const {
			return rtvHeap ? rtvHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
		}

		// 화면 크기에 맞는 DX12 MSAA color render target과 RTV를 생성한다.
		GraphicsResult<void> Initialize(const Dx12Device& sourceDevice, int width, int height);
		// 장면 색상을 back buffer로 resolve 또는 copy하고 두 리소스 상태를 복원한다.
		bool ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList,
			ID3D12GraphicsCommandList7* enhancedCommandList, ID3D12Resource* backBuffer) const;
		// 생성한 DX12 MSAA color 리소스를 해제한다.
		void Reset();
	};
}
