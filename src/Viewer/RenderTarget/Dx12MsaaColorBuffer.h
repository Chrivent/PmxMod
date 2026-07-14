#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12MsaaColorBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> renderTarget;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;

	public:
		// 화면 크기에 맞는 DX12 MSAA color render target과 RTV를 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, int width, int height);
		// MSAA color render target resource를 반환한다.
		ID3D12Resource* ResolveResource() const { return renderTarget.Get(); }
		// RTV heap에서 MSAA color render target view handle을 해석해 반환한다.
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveRtvHandle() const;
		// 생성한 DX12 MSAA color 리소스를 해제한다.
		void Reset();
	};
}
