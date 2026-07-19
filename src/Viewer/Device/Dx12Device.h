#pragma once

#include "Viewer/Device/GraphicsCapabilities.h"
#include "Viewer/Error/GraphicsError.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 디바이스, 명령 큐와 지원 기능 정보를 관리한다.
	class Dx12Device {
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		Microsoft::WRL::ComPtr<ID3D12Device> device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
		UINT msaaSampleCount = 1;
		D3D_SHADER_MODEL maximumShaderModel = D3D_SHADER_MODEL_5_1;
		bool enhancedBarriersSupported = false;

		// 지정한 sample count를 색상과 depth 타깃이 함께 지원하는지 확인한다.
		static bool SupportsMsaaSampleCount(ID3D12Device* device, UINT sampleCount);
		// 디바이스가 지원하는 MSAA sample count를 선택한다.
		static UINT ChooseMsaaSampleCount(ID3D12Device* device);
		// 현재 렌더 타깃 형식에 사용할 수 있는 최대 MSAA sample count를 반환한다.
		static UINT ResolveMaximumMsaaSampleCount(ID3D12Device* device);
		// DX12 feature level을 로그용 문자열로 변환한다.
		static const char* ResolveFeatureLevelName(D3D_FEATURE_LEVEL featureLevel);
		// DX12 shader model을 로그용 문자열로 변환한다.
		static const char* ResolveShaderModelName(D3D_SHADER_MODEL shaderModel);
		// 생성된 디바이스가 지원하는 기능과 한도를 기록한다.
		void UpdateCapabilities(GraphicsCapabilities& capabilities,
			const DXGI_ADAPTER_DESC1& description);
		// 생성한 DX12 디바이스 리소스를 해제한다.
		void Shutdown();

	public:
		~Dx12Device();

		IDXGIFactory6* GetFactory() const { return factory.Get(); }
		ID3D12Device* GetDevice() const { return device.Get(); }
		ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
		UINT GetMsaaSampleCount() const { return msaaSampleCount; }
		D3D_SHADER_MODEL GetMaximumShaderModel() const { return maximumShaderModel; }
		bool SupportsEnhancedBarriers() const { return enhancedBarriersSupported; }

		// DX12 디바이스와 command queue를 생성한다.
		GraphicsError::Result<void> Initialize(GraphicsCapabilities& capabilities);
	};
}
