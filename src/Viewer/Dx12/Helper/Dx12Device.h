#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace Chrivent {
	struct Dx12DeviceInfo {
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		Microsoft::WRL::ComPtr<ID3D12Device> device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	};

	class Dx12Device {
		Dx12DeviceInfo info;

	public:
		const Dx12DeviceInfo& GetInfo() const { return info; }

		// DX12 디바이스와 command queue를 생성한다.
		bool Initialize();
		// 생성한 DX12 디바이스 리소스를 해제한다.
		void Destroy();
	};
}
