#include "Viewer/SwapChain/Dx11SwapChain.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

#include <dxgi1_6.h>

namespace Chrivent {
	GraphicsError::Result<void> Dx11SwapChain::Initialize(ID3D11Device* device, HWND__* window) {
		Reset();
		if (device == nullptr || window == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "swap chain 생성",
				"DirectX 11 device 또는 출력 창을 사용할 수 없습니다"));
		}
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		HRESULT result = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 11 device에서 DXGI device를 가져오지 못했습니다", result, true));
		}
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		result = dxgiDevice->GetAdapter(&adapter);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 11 device의 DXGI adapter를 가져오지 못했습니다", result, true));
		}
		Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
		result = adapter->GetParent(IID_PPV_ARGS(&factory));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"flip-model 스왑체인을 지원하는 DXGI factory를 가져오지 못했습니다",
				result, true));
		}
		const auto description = Dx11DescBuilder::MakeSwapChainDesc();
		Microsoft::WRL::ComPtr<IDXGISwapChain1> createdSwapChain;
		result = factory->CreateSwapChainForHwnd(device, window, &description,
			nullptr, nullptr, &createdSwapChain);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 11 flip-model 스왑체인을 만들지 못했습니다", result, true));
		}
		result = createdSwapChain.As(&swapChain);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"생성한 DXGI 스왑체인 인터페이스를 가져오지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> Dx11SwapChain::Resize(const int width, const int height) const {
		if (!swapChain || width <= 0 || height <= 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "swap chain 크기 변경",
				"스왑체인을 사용할 수 없거나 framebuffer 크기가 올바르지 않습니다"));
		}
		const HRESULT result = swapChain->ResizeBuffers(0, width, height,
			DXGI_FORMAT_UNKNOWN, 0);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 크기 변경",
				"DirectX 11 back buffer 크기를 바꾸지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> Dx11SwapChain::Present() const {
		if (!swapChain) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "swap chain present",
				"DirectX 11 스왑체인을 사용할 수 없습니다"));
		}
		const HRESULT result = swapChain->Present(0, 0);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::PresentationFailed, "swap chain present",
				"DirectX 11 프레임을 표시하지 못했습니다", result, true));
		}
		return {};
	}

	void Dx11SwapChain::Reset() {
		swapChain.Reset();
	}
}
