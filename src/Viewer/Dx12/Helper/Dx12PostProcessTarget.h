#pragma once

#include "Viewer/Dx12/Helper/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12PostProcessTarget {
		Microsoft::WRL::ComPtr<ID3D12Resource> sceneColor;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	public:
		// 화면 크기에 맞는 단일 샘플 장면 색상 텍스처와 SRV를 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, int width, int height);
		// 후처리 입력 장면 색상 리소스를 반환한다.
		ID3D12Resource* ResolveResource() const { return sceneColor.Get(); }
		// 후처리 입력 SRV가 들어 있는 shader-visible heap을 반환한다.
		ID3D12DescriptorHeap* ResolveDescriptorHeap() const { return descriptorHeap.Get(); }
		// 후처리 입력 SRV의 GPU descriptor handle을 반환한다.
		D3D12_GPU_DESCRIPTOR_HANDLE ResolveGpuHandle() const;
		// 생성한 후처리 입력 리소스를 해제한다.
		void Reset();
	};
}
