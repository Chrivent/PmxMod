#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx11PostProcess;

	// D3D11 swapchain back buffer와 장면 MSAA 색상 및 depth target을 소유한다.
	class Dx11RenderTargets {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorMsaa;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneColorMsaaView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;

	public:
		ID3D11Texture2D* ResolveBackBuffer() const { return backBuffer.Get(); }
		ID3D11RenderTargetView* ResolveBackBufferView() const { return backBufferView.Get(); }
		ID3D11Texture2D* ResolveSceneColor() const { return sceneColorMsaa.Get(); }
		ID3D11RenderTargetView* ResolveSceneColorView() const { return sceneColorMsaaView.Get(); }
		ID3D11DepthStencilView* ResolveDepthStencilView() const { return depthStencilView.Get(); }

		// 현재 출력 크기와 MSAA 설정으로 장면 및 후처리 target을 생성한다.
		bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain,
			Dx11PostProcess& postProcess, int width, int height, UINT sampleCount, UINT sampleQuality);
		// swapchain 크기 변경 전에 장면 및 후처리 target을 해제한다.
		void Reset(ID3D11DeviceContext* context, Dx11PostProcess& postProcess);
	};
}
