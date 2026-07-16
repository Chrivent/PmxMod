#include "Viewer/Device/Dx11Device.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Util.h"

#include <algorithm>
#include <dxgi1_6.h>
#include <iterator>

namespace Chrivent {
	bool Dx11Device::ResolveMsaaQuality(const UINT sampleCount, UINT& quality) const {
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
			if (ResolveMsaaQuality(sampleCount, quality))
				return sampleCount;
		}
		return 1;
	}

	bool Dx11Device::Initialize(GraphicsCapabilities& capabilities) {
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
			return false;
		constexpr D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};
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
			HRESULT result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
				featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
				&device, &selectedFeatureLevel, &context);
			if (result == E_INVALIDARG) {
				result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
					&featureLevels[1], 1, D3D11_SDK_VERSION, &device, &selectedFeatureLevel, &context);
			}
			if (FAILED(result))
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
			return true;
		}
		return false;
	}

	void Dx11Device::ResolveMsaaSettings(UINT& sampleCount, UINT& quality,
		GraphicsCapabilities& capabilities) const {
		constexpr UINT preferredSampleCounts[] = { 4u, 2u };
		for (const UINT candidate : preferredSampleCounts) {
			if (!ResolveMsaaQuality(candidate, quality))
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

	bool Dx11Device::CreateSwapChain(HWND__* window, const UINT sampleCount, const UINT sampleQuality) {
		if (!device || window == nullptr)
			return false;
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		if (FAILED(device.As(&dxgiDevice)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		if (FAILED(dxgiDevice->GetAdapter(&adapter)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;
		if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory)))
			return false;
		auto description = Dx11DescBuilder::MakeSwapChainDesc(window, sampleCount, sampleQuality);
		return SUCCEEDED(factory->CreateSwapChain(device.Get(), &description, &swapChain));
	}

	bool Dx11Device::WaitIdle() const {
		if (!device || !context)
			return false;
		D3D11_QUERY_DESC queryDescription{};
		queryDescription.Query = D3D11_QUERY_EVENT;
		Microsoft::WRL::ComPtr<ID3D11Query> query;
		if (FAILED(device->CreateQuery(&queryDescription, &query)))
			return false;
		context->End(query.Get());
		context->Flush();
		while (true) {
			const HRESULT result = context->GetData(query.Get(), nullptr, 0, 0);
			if (result == S_OK)
				return true;
			if (result != S_FALSE)
				return false;
			SwitchToThread();
		}
	}
}
