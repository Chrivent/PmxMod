#include "Viewer/RenderTarget/Dx11RenderTargets.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	GraphicsError::Result<void> Dx11RenderTargets::Initialize(ID3D11Device* device, IDXGISwapChain* swapChain,
		const int width, const int height, const UINT sampleCount, const UINT sampleQuality) {
		if (device == nullptr || swapChain == nullptr || width <= 0 || height <= 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "render target 생성",
				"DirectX 11 device, swapchain 또는 출력 크기가 올바르지 않습니다"));
		}
		HRESULT result = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "back buffer 획득",
				"DirectX 11 swapchain back buffer를 가져오지 못했습니다", result, true));
		}
		result = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferView);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "back buffer view 생성",
				"DirectX 11 back buffer view를 만들지 못했습니다", result, true));
		}
		const auto colorDescription = Dx11DescBuilder::MakeTexture2DDesc(width, height,
			DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, sampleCount, sampleQuality);
		result = device->CreateTexture2D(&colorDescription, nullptr, &sceneColorMsaa);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "MSAA color target 생성",
				"DirectX 11 MSAA color target을 만들지 못했습니다", result, true));
		}
		result = device->CreateRenderTargetView(sceneColorMsaa.Get(), nullptr, &sceneColorMsaaView);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "MSAA color view 생성",
				"DirectX 11 MSAA color view를 만들지 못했습니다", result, true));
		}
		const auto depthDescription = Dx11DescBuilder::MakeTexture2DDesc(width, height,
			DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, sampleCount, sampleQuality);
		result = device->CreateTexture2D(&depthDescription, nullptr, &depthTexture);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "depth target 생성",
				"DirectX 11 depth target을 만들지 못했습니다", result, true));
		}
		result = device->CreateDepthStencilView(depthTexture.Get(), nullptr, &depthStencilView);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "depth stencil view 생성",
				"DirectX 11 depth stencil view를 만들지 못했습니다", result, true));
		}
		return {};
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
