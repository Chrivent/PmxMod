#pragma once

#include "Viewer/Device/Dx12Device.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Synchronization/FrameBuffering.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 스왑체인과 프레임별 back buffer 및 RTV를 관리한다.
	class Dx12SwapChain {
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers[FrameBuffering::dx12BufferCount]{};
		UINT rtvDescriptorSize = 0;
		UINT frameIndex = 0;

		// 스왑체인 back buffer마다 RTV descriptor를 생성한다.
		GraphicsResult<void> CreateRenderTargetViews(const Dx12Device& sourceDevice);

	public:
		UINT GetFrameIndex() const { return frameIndex; }
		ID3D12Resource* GetCurrentBackBuffer() const { return backBuffers[frameIndex].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const;

		// DX12 스왑체인과 back buffer RTV를 생성한다.
		GraphicsResult<void> Initialize(const Dx12Device& sourceDevice, HWND hwnd,
			int width, int height);
		// 창 크기에 맞춰 스왑체인을 다시 생성한다.
		GraphicsResult<void> Resize(const Dx12Device& sourceDevice, int width, int height);
		// swap chain의 현재 back buffer를 화면에 표시한다.
		GraphicsResult<void> Present();
		// 생성한 DX12 스왑체인 리소스를 해제한다.
		void Reset();
	};
}
