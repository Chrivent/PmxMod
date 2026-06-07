#pragma once

#include "Dx12Device.h"

#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12SwapChain {
		static constexpr UINT kFrameCount = 2;

		Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kFrameCount> backBuffers{};
		UINT rtvDescriptorSize = 0;
		UINT frameIndex = 0;

		bool CreateRenderTargetViews(const Dx12DeviceInfo& deviceInfo);

	public:
		ID3D12Resource* GetCurrentBackBuffer() const { return backBuffers[frameIndex].Get(); }

		// DX12 스왑체인과 back buffer RTV를 생성한다.
		bool Initialize(const Dx12DeviceInfo& deviceInfo, HWND hwnd, int width, int height);
		// 창 크기에 맞춰 스왑체인을 다시 생성한다.
		bool Resize(const Dx12DeviceInfo& deviceInfo, int width, int height);
		// swap chain의 현재 back buffer를 화면에 표시한다.
		bool Present();
		// 생성한 DX12 스왑체인 리소스를 해제한다.
		void Destroy();
		// 현재 back buffer의 RTV descriptor handle을 계산한다.
		D3D12_CPU_DESCRIPTOR_HANDLE CalculateCurrentRtvHandle() const;
	};
}
