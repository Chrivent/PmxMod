#include "Viewer/RenderTarget/Dx11RenderTargets.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	bool Dx11RenderTargets::Initialize(ID3D11Device* device, IDXGISwapChain* swapChain,
		const int width, const int height, const UINT sampleCount, const UINT sampleQuality) {
		if (device == nullptr || swapChain == nullptr || width <= 0 || height <= 0)
			return false;
		if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
			return false;
		if (FAILED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferView)))
			return false;
		const auto colorDescription = Dx11DescBuilder::MakeTexture2DDesc(width, height,
			DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, sampleCount, sampleQuality);
		if (FAILED(device->CreateTexture2D(&colorDescription, nullptr, &sceneColorMsaa))
			|| FAILED(device->CreateRenderTargetView(sceneColorMsaa.Get(), nullptr, &sceneColorMsaaView)))
			return false;
		const auto depthDescription = Dx11DescBuilder::MakeTexture2DDesc(width, height,
			DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, sampleCount, sampleQuality);
		if (FAILED(device->CreateTexture2D(&depthDescription, nullptr, &depthTexture))
			|| FAILED(device->CreateDepthStencilView(depthTexture.Get(), nullptr, &depthStencilView)))
			return false;
		return true;
	}

	void Dx11RenderTargets::Reset(ID3D11DeviceContext* context) {
		if (context != nullptr)
			context->OMSetRenderTargets(0, nullptr, nullptr);
		backBuffer.Reset();
		backBufferView.Reset();
		sceneColorMsaa.Reset();
		sceneColorMsaaView.Reset();
		depthStencilView.Reset();
		depthTexture.Reset();
	}
}
