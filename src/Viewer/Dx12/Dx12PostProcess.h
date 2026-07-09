#pragma once

#include "Viewer/Dx12/Dx12PostProcessTarget.h"
#include "Viewer/PostProcess.h"

#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12CommandContext;
	class Dx12Device;
	class Dx12Pipeline;
	class Dx12SwapChain;

	class Dx12PostProcess : public PostProcess {
		std::vector<Dx12PostProcessTarget> targets;
		Microsoft::WRL::ComPtr<ID3D12Resource> depth;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthDsvHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> focusHistory[2];
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> focusHistoryRtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> focusHistorySrvHeap;
		bool focusHistoryInitialized = false;
		int focusHistoryIndex = 0;

		// MSAA 화면 색상을 단일 샘플 back buffer로 복사한다.
		static void ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
			ID3D12Resource* msaaColor, const Dx12Device& sourceDevice, const Dx12CommandContext& commandContext);
		// 현재 화면 크기에 맞는 viewport와 scissor rect를 명령 목록에 적용한다.
		static void ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList, int width, int height);
		// 후처리 depth-only pass에 사용할 단일 샘플 depth target을 생성한다.
		bool CreateDepthTarget(const Dx12Device& sourceDevice, int width, int height);
		// DOF용 1x1 초점 히스토리 ping-pong target을 생성한다.
		bool CreateFocusHistoryTargets(const Dx12Device& sourceDevice);
		// DOF용 초점 히스토리 갱신 pass 입력 descriptor를 갱신한다.
		void UpdateFocusHistoryShaderResources(const Dx12Device& sourceDevice, int readIndex) const;
		// DOF용 초점 히스토리 갱신 pass를 실행한다.
		void UpdateFocusHistory(ID3D12GraphicsCommandList* commandList, const Dx12Device& sourceDevice,
			const Dx12CommandContext& commandContext, const Dx12Pipeline& pipeline, int width, int height);
		// 후처리 depth-only pass용 DSV handle을 반환한다.
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveDepthDsvHandle() const;
		// DOF용 초점 히스토리 RTV handle을 반환한다.
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveFocusHistoryRtvHandle(const Dx12Device& sourceDevice, int index) const;
		// DOF용 초점 히스토리 입력 descriptor table handle을 반환한다.
		D3D12_GPU_DESCRIPTOR_HANDLE ResolveFocusHistoryGpuHandle() const;

	public:
		Dx12PostProcess() = default;
		~Dx12PostProcess() override = default;

		Dx12PostProcess(const Dx12PostProcess&) = delete;
		Dx12PostProcess& operator=(const Dx12PostProcess&) = delete;
		Dx12PostProcess(Dx12PostProcess&&) = delete;
		Dx12PostProcess& operator=(Dx12PostProcess&&) = delete;

		// 화면 크기에 맞는 DX12 후처리 색상/depth/focus history target을 생성한다.
		bool InitializeTargets(const Dx12Device& sourceDevice, int width, int height);
		// 체크된 포스트 프로세스 효과들을 DX12 ping-pong chain으로 컴파일한다.
		bool Load(const Dx12Device& sourceDevice, Dx12Pipeline& pipeline,
			const std::vector<const EffectDefinition*>& effects);
		// DX12 후처리 pipeline과 선택 effect 목록만 해제한다.
		void ClearPipelines(Dx12Pipeline& pipeline);
		// DX12 포스트 프로세스용 단일 샘플 depth-only pass를 시작한다.
		bool BeginDepthPass(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext, int width, int height) const;
		// DX12 포스트 프로세스용 단일 샘플 depth-only pass를 종료한다.
		void EndDepthPass(ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext) const;
		// 준비된 후처리 pipeline chain으로 MSAA 화면을 back buffer에 출력한다.
		void Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer, ID3D12Resource* msaaColor,
			const Dx12Device& sourceDevice, const Dx12CommandContext& commandContext,
			const Dx12SwapChain& swapChain, const Dx12Pipeline& pipeline, int width, int height);
		// 생성한 DX12 후처리 리소스를 해제한다.
		void Reset() override;
	};
}
