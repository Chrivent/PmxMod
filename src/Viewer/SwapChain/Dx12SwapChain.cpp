#include "Viewer/SwapChain/Dx12SwapChain.h"

namespace Chrivent {
	GraphicsResult<void> Dx12SwapChain::CreateRenderTargetViews(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetDevice() || !swapChain) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "back buffer RTV 생성",
				"DirectX 12 device 또는 스왑체인을 사용할 수 없습니다"));
		}
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = FrameBuffering::dx12BufferCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		HRESULT result = sourceDevice.GetDevice()->CreateDescriptorHeap(&rtvHeapDesc,
			IID_PPV_ARGS(&rtvHeap));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "back buffer RTV heap 생성",
				"DirectX 12 RTV descriptor heap을 만들지 못했습니다", result, true));
		}
		rtvDescriptorSize = sourceDevice.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT index = 0; index < FrameBuffering::dx12BufferCount; index++) {
			result = swapChain->GetBuffer(index, IID_PPV_ARGS(&backBuffers[index]));
			if (FAILED(result)) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "back buffer 조회",
					"DirectX 12 스왑체인 back buffer를 가져오지 못했습니다", result, true));
			}
			sourceDevice.GetDevice()->CreateRenderTargetView(backBuffers[index].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
		return {};
	}

	GraphicsResult<void> Dx12SwapChain::Initialize(const Dx12Device& sourceDevice,
		const HWND hwnd, const int width, const int height) {
		Reset();
		if (!sourceDevice.GetFactory() || !sourceDevice.GetDevice()
			|| !sourceDevice.GetCommandQueue() || !hwnd || width <= 0 || height <= 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "swap chain 생성",
				"DirectX 12 device, command queue, 출력 창 또는 크기가 올바르지 않습니다"));
		}
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.BufferCount = FrameBuffering::dx12BufferCount;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> baseSwapChain;
		HRESULT result = sourceDevice.GetFactory()->CreateSwapChainForHwnd(sourceDevice.GetCommandQueue(),
			hwnd, &swapChainDesc, nullptr, nullptr, &baseSwapChain);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 12 flip-model 스왑체인을 만들지 못했습니다", result, true));
		}
		result = sourceDevice.GetFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InitializationFailed, "창 연결 설정",
				"DirectX 12 창 연결 설정을 적용하지 못했습니다", result, true));
		}
		result = baseSwapChain.As(&swapChain);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 인터페이스 조회",
				"IDXGISwapChain3 인터페이스를 가져오지 못했습니다", result, true));
		}
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(sourceDevice);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::GetCurrentRtvHandle() const {
		if (!rtvHeap)
			return {};
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize;
		return handle;
	}

	GraphicsResult<void> Dx12SwapChain::Resize(const Dx12Device& sourceDevice,
		const int width, const int height) {
		if (!swapChain || width <= 0 || height <= 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "swap chain 크기 변경",
				"스왑체인을 사용할 수 없거나 framebuffer 크기가 올바르지 않습니다"));
		}
		for (auto& backBuffer : backBuffers)
			backBuffer.Reset();
		const HRESULT result = swapChain->ResizeBuffers(FrameBuffering::dx12BufferCount,
			width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 크기 변경",
				"DirectX 12 back buffer 크기를 바꾸지 못했습니다", result, true));
		}
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(sourceDevice);
	}

	GraphicsResult<void> Dx12SwapChain::Present() {
		if (!swapChain) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "swap chain present",
				"DirectX 12 스왑체인을 사용할 수 없습니다"));
		}
		const HRESULT result = swapChain->Present(0, 0);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::PresentationFailed, "swap chain present",
				"DirectX 12 프레임을 표시하지 못했습니다", result, true));
		}
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return {};
	}

	void Dx12SwapChain::Reset() {
		for (auto& backBuffer : backBuffers)
			backBuffer.Reset();
		rtvHeap.Reset();
		swapChain.Reset();
		rtvDescriptorSize = 0;
		frameIndex = 0;
	}
}
