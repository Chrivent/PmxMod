#pragma once

#include "Viewer/Buffer/Dx12Buffer.h"
#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12SwapChain;
	struct PostProcessFrameData;
	
	class Dx12PostProcessTarget {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

	public:
		// 화면 크기에 맞는 단일 샘플 후처리 렌더 타깃과 RTV를 생성한다.
		bool Initialize(const Dx12Device& sourceDevice, int width, int height, DXGI_FORMAT targetFormat);
		// 후처리 렌더 타깃 리소스를 반환한다.
		ID3D12Resource* ResolveResource() const { return resource.Get(); }
		// 후처리 렌더 타깃 형식을 반환한다.
		DXGI_FORMAT ResolveFormat() const { return format; }
		// 후처리 렌더 타깃 RTV의 CPU descriptor handle을 반환한다.
		D3D12_CPU_DESCRIPTOR_HANDLE ResolveRtvHandle() const;
		// 생성한 후처리 입력 리소스를 해제한다.
		void Reset();
	};

	struct Dx12PostProcessResource {
		Dx12PostProcessTarget targets[2];
		size_t historyIndex = 0;
		bool historyInitialized = false;
	};

	class Dx12PostProcess : public PostProcess {
		static constexpr size_t frameDataBufferCount = 2;
		Dx12PostProcessTarget sceneColor;
		Dx12PostProcessTarget sceneVelocity;
		std::vector<Dx12PostProcessResource> resources;
		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> inputDescriptorHeaps;
		Microsoft::WRL::ComPtr<ID3D12Resource> depth;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthDsvHeap;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> postProcessRootSignature;
		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> postProcessPipelineStates;
		Dx12Buffer frameDataBuffers[frameDataBufferCount];
		int targetWidth = 0;
		int targetHeight = 0;

		// MSAA 화면 색상을 단일 샘플 back buffer로 복사한다.
		static void ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
			ID3D12Resource* msaaColor, const Dx12Device& sourceDevice,
			const Dx12CommandContext& commandContext);
		// 현재 화면 크기에 맞는 viewport와 scissor rect를 command list에 적용한다.
		static void ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList, int width, int height);
		// 후처리 depth-only pass에 사용할 단일 샘플 depth target을 생성한다.
		bool CreateDepthTarget(const Dx12Device& sourceDevice, int width, int height);
		// 패키지가 선언한 transient/history target을 생성한다.
		bool CreateEffectResources(const Dx12Device& sourceDevice);
		// pass별 shader input descriptor heap을 frame 수만큼 생성한다.
		bool CreateInputDescriptorHeaps(const Dx12Device& sourceDevice);
		// 현재 frame의 pass 입력 descriptor를 실행 계획에 맞게 갱신한다.
		void UpdateInputDescriptors(const Dx12Device& sourceDevice, size_t frameIndex, size_t passIndex) const;
		// 모든 history target을 최초 사용 전에 0으로 지운다.
		void InitializeHistories(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext);
		// 후처리 공통 root signature를 생성한다.
		bool CreatePostProcessRootSignature(const Dx12Device& sourceDevice);
		// HLSL pass 하나를 지정한 출력 형식의 pipeline state로 생성한다.
		bool CreatePipelineState(const Dx12Device& sourceDevice, const EffectPassDefinition& pass,
			DXGI_FORMAT format, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState) const;
		// 공통 실행 계획의 모든 DX12 pipeline state를 생성한다.
		bool CreatePipelines(const Dx12Device& sourceDevice);
		// pass에 대응하는 shader-visible descriptor heap을 반환한다.
		ID3D12DescriptorHeap* ResolveInputDescriptorHeap(size_t frameIndex, size_t passIndex) const;
		// 입력 경로에 대응하는 DX12 resource와 SRV 형식을 반환한다.
		ID3D12Resource* ResolveInputResource(const PostProcessPassInputRoute& input, DXGI_FORMAT& format) const;
		// 출력 경로에 대응하는 DX12 target을 반환한다.
		Dx12PostProcessTarget* ResolveOutputTarget(const PostProcessPassRoute& route);
		// pass 출력 크기를 계산한다.
		void ResolveOutputExtent(const PostProcessPassRoute& route, int& width, int& height) const;
		// history 출력 pass가 끝난 뒤 read/write 인덱스를 전환한다.
		void AdvanceHistory(const PostProcessPassRoute& route);
		// pass pipeline과 root signature를 해제한다.
		void ResetPipelines();
		// 선언 기반 effect target과 descriptor를 해제한다.
		void ResetEffectResources();

	public:
		// 현재 크기와 선택된 effect 선언에 맞는 DX12 후처리 target을 생성한다.
		bool InitializeTargets(const Dx12Device& sourceDevice, int width, int height);
		// 체크된 후처리 effect 선언으로 DX12 실행 리소스와 pipeline을 생성한다.
		bool Load(const Dx12Device& sourceDevice, const std::vector<const EffectDefinition*>& effects);
		// DX12 후처리 pipeline과 선택 effect 목록만 해제한다.
		void ClearPipelines();
		// 후처리 depth-only pass를 시작한다.
		bool BeginDepthPass(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext, int width, int height) const;
		// 후처리 depth-only pass를 종료한다.
		void EndDepthPass(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext) const;
		// 준비된 실행 계획으로 장면 색상을 swapchain back buffer에 그린다.
		void Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
			ID3D12Resource* msaaColor, const Dx12Device& sourceDevice,
			const Dx12CommandContext& commandContext, const Dx12SwapChain& swapChain,
			int width, int height, const PostProcessFrameData& frameData);
		// 다음 후처리 프레임에서 모든 DX12 history를 0으로 초기화한다.
		void ResetHistory() override;
		// 생성한 DX12 후처리 리소스를 해제한다.
		void Reset() override;
	};
}
