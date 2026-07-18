#pragma once

#include "Viewer/Device/GraphicsCapabilities.h"
#include "Viewer/Error/GraphicsError.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 device, immediate context와 swapchain의 생성 및 동기화를 담당한다.
	class Dx11Device {
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

		// 지정한 sample count를 색상과 depth 형식이 함께 지원하는지 확인한다.
		bool TryGetMsaaQuality(UINT sampleCount, UINT& quality) const;
		// 현재 device가 지원하는 최대 MSAA sample count를 반환한다.
		UINT ResolveMaximumMsaaSampleCount() const;

	public:
		ID3D11Device* GetDevice() const { return device.Get(); }
		ID3D11DeviceContext* GetContext() const { return context.Get(); }
		IDXGISwapChain* GetSwapChain() const { return swapChain.Get(); }

		// 고성능 DXGI 어댑터를 선택해 D3D11 device와 immediate context를 생성한다.
		GraphicsResult<void> Initialize(GraphicsCapabilities& capabilities);
		// 현재 device의 4→2→1 정책으로 MSAA 설정과 capability 값을 확정한다.
		void SelectMsaaSettings(UINT& sampleCount, UINT& quality,
			GraphicsCapabilities& capabilities) const;
		// 지정한 Win32 창에 단일 샘플 back buffer를 사용하는 swapchain을 생성한다.
		GraphicsResult<void> CreateSwapChain(HWND__* window);
		// immediate context에 제출한 작업이 끝날 때까지 기다린다.
		GraphicsResult<void> WaitIdle() const;
	};
}
