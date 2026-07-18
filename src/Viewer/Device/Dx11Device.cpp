#include "Viewer/Device/Dx11Device.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Util.h"

#include <algorithm>
#include <dxgi1_6.h>
#include <iterator>

namespace Chrivent {
	bool Dx11Device::TryGetMsaaQuality(const UINT sampleCount, UINT& quality) const {
		quality = 0;
		if (!device || sampleCount <= 1)
			return sampleCount == 1;
		UINT colorQuality = 0;
		UINT depthQuality = 0;
		if (FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,
			sampleCount, &colorQuality)) || colorQuality == 0
			|| FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT,
				sampleCount, &depthQuality)) || depthQuality == 0)
			return false;
		quality = std::min(colorQuality, depthQuality) - 1;
		return true;
	}

	UINT Dx11Device::ResolveMaximumMsaaSampleCount() const {
		constexpr UINT sampleCounts[] = { 32u, 16u, 8u, 4u, 2u };
		for (const UINT sampleCount : sampleCounts) {
			UINT quality = 0;
			if (TryGetMsaaQuality(sampleCount, quality))
				return sampleCount;
		}
		return 1;
	}

	GraphicsResult<void> Dx11Device::Initialize(GraphicsCapabilities& capabilities) {
		swapChain.Reset();
		context.Reset();
		device.Reset();
		capabilities = {};
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		const HRESULT factoryResult = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
		if (FAILED(factoryResult)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::InitializationFailed, "DXGI factory 생성",
				"DirectX 11 어댑터 검색용 factory를 만들지 못했습니다", factoryResult, true));
		}
		constexpr D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};
		HRESULT lastDeviceResult = E_FAIL;
		for (UINT index = 0; ; index++) {
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (FAILED(factory->EnumAdapterByGpuPreference(index,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))))
				break;
			DXGI_ADAPTER_DESC1 description{};
			if (FAILED(adapter->GetDesc1(&description))
				|| (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
				continue;
			D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
			lastDeviceResult = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
				featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
				&device, &selectedFeatureLevel, &context);
			if (lastDeviceResult == E_INVALIDARG) {
				lastDeviceResult = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
					&featureLevels[1], 1, D3D11_SDK_VERSION, &device, &selectedFeatureLevel, &context);
			}
			if (FAILED(lastDeviceResult))
				continue;
			capabilities.apiName = "Direct3D 11";
			capabilities.apiVersion = selectedFeatureLevel == D3D_FEATURE_LEVEL_11_1
				? "Feature Level 11.1" : "Feature Level 11.0";
			capabilities.shaderVersion = "Shader Model 5.0";
			capabilities.gpuName = Util::WStringToUtf8(description.Description);
			capabilities.gpuType = description.DedicatedVideoMemory > 0 ? "discrete" : "integrated";
			capabilities.uniformBufferAlignment = 16;
			capabilities.maxTextureBindings = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
			capabilities.shaderModelMajor = 5;
			return {};
		}
		return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
			GraphicsErrorCode::InitializationFailed, "device 초기화",
			"DirectX 11을 지원하는 고성능 그래픽 어댑터를 찾지 못했습니다", lastDeviceResult, true));
	}

	void Dx11Device::SelectMsaaSettings(UINT& sampleCount, UINT& quality,
		GraphicsCapabilities& capabilities) const {
		constexpr UINT preferredSampleCounts[] = { 4u, 2u };
		for (const UINT candidate : preferredSampleCounts) {
			if (!TryGetMsaaQuality(candidate, quality))
				continue;
			sampleCount = candidate;
			capabilities.maxSampleCount = ResolveMaximumMsaaSampleCount();
			capabilities.activeSampleCount = sampleCount;
			return;
		}
		sampleCount = 1;
		quality = 0;
		capabilities.maxSampleCount = ResolveMaximumMsaaSampleCount();
		capabilities.activeSampleCount = sampleCount;
	}

	GraphicsResult<void> Dx11Device::CreateSwapChain(HWND__* window) {
		if (!device || window == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "swap chain 생성",
				"DirectX 11 device 또는 출력 창을 사용할 수 없습니다"));
		}
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		HRESULT result = device.As(&dxgiDevice);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 11 device에서 DXGI device를 가져오지 못했습니다", result, true));
		}
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		result = dxgiDevice->GetAdapter(&adapter);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 11 device의 DXGI adapter를 가져오지 못했습니다", result, true));
		}
		Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
		result = adapter->GetParent(IID_PPV_ARGS(&factory));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"flip-model swap chain을 지원하는 DXGI factory를 가져오지 못했습니다", result, true));
		}
		const auto description = Dx11DescBuilder::MakeSwapChainDesc();
		Microsoft::WRL::ComPtr<IDXGISwapChain1> newSwapChain;
		result = factory->CreateSwapChainForHwnd(device.Get(), window, &description,
			nullptr, nullptr, &newSwapChain);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"DirectX 11 flip-model swap chain을 만들지 못했습니다", result, true));
		}
		result = newSwapChain.As(&swapChain);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"생성한 DXGI swap chain 인터페이스를 가져오지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> Dx11Device::WaitIdle() const {
		if (!device || !context) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "GPU 대기",
				"DirectX 11 device 또는 immediate context를 사용할 수 없습니다"));
		}
		D3D11_QUERY_DESC queryDescription{};
		queryDescription.Query = D3D11_QUERY_EVENT;
		Microsoft::WRL::ComPtr<ID3D11Query> query;
		const HRESULT queryResult = device->CreateQuery(&queryDescription, &query);
		if (FAILED(queryResult)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::SynchronizationFailed, "GPU 대기",
				"DirectX 11 완료 확인 query를 만들지 못했습니다", queryResult, true));
		}
		context->End(query.Get());
		context->Flush();
		while (true) {
			const HRESULT result = context->GetData(query.Get(), nullptr, 0, 0);
			if (result == S_OK)
				return {};
			if (result != S_FALSE) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
					GraphicsErrorCode::SynchronizationFailed, "GPU 대기",
					"DirectX 11 GPU 완료 상태를 가져오지 못했습니다", result, true));
			}
			SwitchToThread();
		}
	}
}
