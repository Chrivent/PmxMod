#include "Dx12Device.h"

namespace Chrivent {
	bool Dx12Device::Initialize() {
		Destroy();
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&info.factory))))
			return false;
		for (UINT index = 0; ; index++) {
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (FAILED(info.factory->EnumAdapterByGpuPreference(
				index,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(&adapter))))
				break;
			DXGI_ADAPTER_DESC1 desc{};
			if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
				continue;
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&info.device)))) {
				info.adapter = adapter;
				break;
			}
		}
		if (!info.device)
			return false;
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(info.device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&info.commandQueue))))
			return false;
		return true;
	}

	void Dx12Device::Destroy() {
		info.commandQueue.Reset();
		info.device.Reset();
		info.adapter.Reset();
		info.factory.Reset();
	}
}
