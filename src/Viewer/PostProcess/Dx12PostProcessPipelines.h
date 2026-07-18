#pragma once

#include "Viewer/Device/Dx12Device.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"

#include <span>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 후처리 공통 root signature와 패스별 pipeline state를 소유한다.
	class Dx12PostProcessPipelines {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates;

		// 후처리 입력 계약에 맞는 공통 root signature를 생성한다.
		bool CreateRootSignature(const Dx12Device& sourceDevice);
		// HLSL 패스 하나를 지정한 출력 형식의 pipeline state로 생성한다.
		bool CreatePipelineState(const Dx12Device& sourceDevice, const ShaderProgramDefinition& program,
			DXGI_FORMAT format, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState) const;

	public:
		ID3D12RootSignature* GetRootSignature() const { return rootSignature.Get(); }
		ID3D12PipelineState* TryGetPipelineState(const size_t index) const {
			return index < pipelineStates.size() ? pipelineStates[index].Get() : nullptr;
		}
		size_t GetCount() const { return pipelineStates.size(); }

		// 패스와 출력 형식 목록을 검증한 뒤 모든 DX12 후처리 파이프라인을 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, std::span<const ShaderProgramDefinition> programs,
			std::span<const DXGI_FORMAT> formats);
		// 다른 DX12 후처리 파이프라인 묶음과 소유권을 교환한다.
		void Swap(Dx12PostProcessPipelines& other) noexcept;
		// 생성한 root signature와 pipeline state를 해제한다.
		void Reset();
	};
}
