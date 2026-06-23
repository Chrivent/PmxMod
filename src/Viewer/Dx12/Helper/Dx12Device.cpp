#include "Viewer/Dx12/Helper/Dx12Device.h"

#include <iostream>

namespace Chrivent {
	UINT Dx12Device::ChooseMsaaSampleCount(ID3D12Device* device) {
		if (device == nullptr)
			return 1;
		constexpr UINT sampleCounts[] = { 4u, 2u };
		for (const UINT sampleCount : sampleCounts) {
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS colorQualityLevels{};
			colorQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			colorQualityLevels.SampleCount = sampleCount;
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS depthQualityLevels{};
			depthQualityLevels.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthQualityLevels.SampleCount = sampleCount;
			const bool supportsColor = SUCCEEDED(device->CheckFeatureSupport(
				D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
				&colorQualityLevels,
				sizeof(colorQualityLevels))) && colorQualityLevels.NumQualityLevels > 0;
			const bool supportsDepth = SUCCEEDED(device->CheckFeatureSupport(
				D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
				&depthQualityLevels,
				sizeof(depthQualityLevels))) && depthQualityLevels.NumQualityLevels > 0;
			if (supportsColor && supportsDepth)
				return sampleCount;
		}
		return 1;
	}

	void Dx12Device::PrintGpuInfo(const DXGI_ADAPTER_DESC1& description) {
		std::wcout << L"dx12_gpu=" << description.Description << L'\n';
		std::wcout << L"dx12_gpu_type=" << (description.DedicatedVideoMemory > 0 ? L"discrete" : L"integrated") << L'\n';
	}

	bool Dx12Device::Initialize() {
		Destroy();
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
			return false;
		for (UINT index = 0; ; index++) {
			Microsoft::WRL::ComPtr<IDXGIAdapter1> newAdapter;
			if (FAILED(factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&newAdapter))))
				break;
			DXGI_ADAPTER_DESC1 desc{};
			if (FAILED(newAdapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
				continue;
			if (SUCCEEDED(D3D12CreateDevice(newAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
				adapter = newAdapter;
				break;
			}
		}
		if (!device)
			return false;
		DXGI_ADAPTER_DESC1 description{};
		if (SUCCEEDED(adapter->GetDesc1(&description)))
			PrintGpuInfo(description);
		msaaSampleCount = ChooseMsaaSampleCount(device.Get());
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue))))
			return false;
		return true;
	}

	void Dx12Device::Destroy() {
		commandQueue.Reset();
		device.Reset();
		adapter.Reset();
		factory.Reset();
		msaaSampleCount = 1;
	}
}
