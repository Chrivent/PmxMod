#pragma once

#include "Viewer/Dx12/Helper/Dx12Device.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12PostProcessTarget {
		Microsoft::WRL::ComPtr<ID3D12Resource> sceneColor;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

	public:
		// 화면 크기에 맞는 단일 샘플 후처리 색상 텍스처와 SRV/RTV를 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, int width, int height, DXGI_FORMAT targetFormat);
		// 후처리 색상 리소스를 반환한다.
		ID3D12Resource* ResolveResource() const { return sceneColor.Get(); }
		// 후처리 색상 리소스 형식을 반환한다.
		DXGI_FORMAT ResolveFormat() const { return format; }
		// 후처리 출력 RTV의 CPU descriptor handle을 반환한다.
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveRtvHandle() const;
		// 생성한 후처리 입력 리소스를 해제한다.
		void Reset();
	};
}
