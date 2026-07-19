#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 후처리 장면 입력용 단일 샘플 depth 리소스와 DSV를 관리한다.
	class Dx12PostProcessDepthTarget {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

	public:
		ID3D12Resource* GetResource() const { return resource.Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const {
			return dsvHeap ? dsvHeap->GetCPUDescriptorHandleForHeapStart()
				: D3D12_CPU_DESCRIPTOR_HANDLE{};
		}

		// 화면 크기에 맞는 shader-readable depth target과 DSV를 생성한다.
		GraphicsResult<void> Initialize(const Dx12Device& sourceDevice, int width, int height);
		// 생성한 depth target 리소스를 해제한다.
		void Reset();
		// 두 depth target의 소유 리소스를 교환한다.
		void Swap(Dx12PostProcessDepthTarget& other) noexcept;
	};
}
