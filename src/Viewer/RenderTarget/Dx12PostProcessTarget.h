#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 후처리 출력 texture와 RTV descriptor를 함께 관리한다.
	class Dx12PostProcessTarget {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

	public:
		// 화면 크기에 맞는 단일 샘플 후처리 렌더 타깃과 RTV를 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, int width, int height, DXGI_FORMAT targetFormat);
		ID3D12Resource* ResolveResource() const { return resource.Get(); }
		DXGI_FORMAT ResolveFormat() const { return format; }
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveRtvHandle() const;

		// 생성한 후처리 입력 리소스를 해제한다.
		void Reset();
	};

	// D3D12 후처리 리소스의 ping-pong 출력 타깃을 보관한다.
	struct Dx12PostProcessResource {
		Dx12PostProcessTarget targets[2];
	};
}
