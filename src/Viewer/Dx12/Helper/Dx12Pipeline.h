#pragma once

#include "Dx12Device.h"

#include <d3d12.h>
#include <filesystem>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12Pipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> modelRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelPipelineState;

		// 모델 셰이더의 리소스 배치와 맞는 root signature를 생성한다.
		bool CreateModelRootSignature(const Dx12DeviceInfo& deviceInfo);
		// 모델 렌더링용 graphics pipeline state를 생성한다.
		bool CreateModelPipelineState(const Dx12DeviceInfo& deviceInfo, const std::filesystem::path& shaderDir);

	public:
		// DX12 모델 렌더링에 필요한 root signature와 pipeline state를 초기화한다.
		bool Initialize(const Dx12DeviceInfo& deviceInfo, const std::filesystem::path& shaderDir);
		// 생성한 DX12 pipeline 리소스를 해제한다.
		void Destroy();

		ID3D12RootSignature* GetModelRootSignature() const { return modelRootSignature.Get(); }
		ID3D12PipelineState* GetModelPipelineState() const { return modelPipelineState.Get(); }
	};
}
