#pragma once

#include "Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12DepthBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

	public:
		// 화면 크기에 맞는 DX12 depth stencil buffer와 DSV를 생성한다.
		bool Initialize(const Dx12Device& deviceInfo, int width, int height);
		// DSV heap에서 depth stencil view handle을 해석해 반환한다.
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveDsvHandle() const;
		// 생성한 DX12 depth stencil 리소스를 해제한다.
		void Destroy();
	};
}
