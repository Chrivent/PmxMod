#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 장면 렌더링용 depth stencil 리소스와 view를 관리한다.
	class Dx12DepthBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const {
			return dsvHeap ? dsvHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
		}

		// 화면 크기에 맞는 DX12 depth stencil buffer와 DSV를 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, int width, int height);
		// 생성한 DX12 depth stencil 리소스를 해제한다.
		void Reset();
	};
}
