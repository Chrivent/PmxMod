#pragma once

#include "Viewer/Error/GraphicsError.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 스왑체인의 생성, 크기 변경과 화면 표시를 관리한다.
	class Dx11SwapChain {
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

	public:
		IDXGISwapChain* GetSwapChain() const { return swapChain.Get(); }

		// 지정한 D3D11 device와 Win32 창에 flip-model 스왑체인을 생성한다.
		GraphicsResult<void> Initialize(ID3D11Device* device, HWND__* window);
		// 창 크기에 맞춰 스왑체인 back buffer를 다시 생성한다.
		GraphicsResult<void> Resize(int width, int height) const;
		// 현재 back buffer를 화면에 표시한다.
		GraphicsResult<void> Present() const;
		// 생성한 스왑체인 리소스를 해제한다.
		void Reset();
	};
}
