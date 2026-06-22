#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12Device {
		// 디바이스가 지원하는 MSAA sample count를 선택한다.
		static UINT ChooseMsaaSampleCount(ID3D12Device* device);
		// 선택된 DXGI 어댑터 정보를 공통 GPU 로그 형식으로 출력한다.
		static void PrintGpuInfo(const DXGI_ADAPTER_DESC1& description);

	public:
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		Microsoft::WRL::ComPtr<ID3D12Device> device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
		UINT msaaSampleCount = 1;

		// DX12 디바이스와 command queue를 생성한다.
		bool Initialize();
		// 생성한 DX12 디바이스 리소스를 해제한다.
		void Destroy();
	};
}
