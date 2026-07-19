#pragma once

#include "Viewer/Error/GraphicsError.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 swapchain back buffer와 장면 MSAA 색상 및 depth target을 소유한다.
	class Dx11RenderTargets {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorMsaa;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneColorMsaaView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;

	public:
		ID3D11Texture2D* GetBackBuffer() const { return backBuffer.Get(); }
		ID3D11RenderTargetView* GetBackBufferView() const { return backBufferView.Get(); }
		ID3D11Texture2D* GetSceneColor() const { return sceneColorMsaa.Get(); }
		ID3D11RenderTargetView* GetSceneColorView() const { return sceneColorMsaaView.Get(); }
		ID3D11DepthStencilView* GetDepthStencilView() const { return depthStencilView.Get(); }

		// 현재 출력 크기와 MSAA 설정으로 장면 target을 생성한다.
		GraphicsResult<void> Initialize(ID3D11Device* device, IDXGISwapChain* swapChain,
			int width, int height, UINT sampleCount, UINT sampleQuality);
		// swapchain 크기 변경 전에 장면 target을 해제한다.
		void Reset(ID3D11DeviceContext* context);
	};
}
