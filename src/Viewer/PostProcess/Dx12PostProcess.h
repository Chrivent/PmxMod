#pragma once

#include "Viewer/Buffer/Dx12Buffer.h"
#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/PostProcess/PostProcess.h"
#include "Viewer/PostProcess/Dx12PostProcessPipelines.h"
#include "Viewer/RenderTarget/Dx12MsaaColorBuffer.h"
#include "Viewer/RenderTarget/Dx12PostProcessTarget.h"

#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12SwapChain;
	struct PostProcessFrameData;
	
	// 공통 실행 계획을 D3D12 파이프라인과 명령 목록으로 실행한다.
	class Dx12PostProcess : public PostProcess {
		// D3D12 후처리 리소스의 ping-pong 출력 타깃을 보관한다.
		struct Resource {
			Dx12PostProcessTarget targets[2];
		};

		Dx12PostProcessTarget sceneColor;
		Dx12PostProcessTarget sceneVelocity;
		std::vector<Resource> resources;
		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> inputDescriptorHeaps;
		Microsoft::WRL::ComPtr<ID3D12Resource> depth;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthDsvHeap;
		Dx12PostProcessPipelines pipelines;
		Dx12Buffer frameDataBuffers[FrameBuffering::dx12BufferCount];
		Dx12Buffer parameterDataBuffers[FrameBuffering::dx12BufferCount];
		int targetWidth = 0;
		int targetHeight = 0;

		// 후처리 장면 입력 패스에 사용할 단일 샘플 depth target을 생성한다.
		bool CreateDepthTarget(const Dx12Device& sourceDevice, int width, int height);
		// 패키지가 선언한 transient/history target을 생성한다.
		bool CreateEffectResources(const Dx12Device& sourceDevice);
		// pass별 shader input descriptor heap을 frame 수만큼 생성한다.
		bool CreateInputDescriptorHeaps(const Dx12Device& sourceDevice);
		// frame별 모든 pass 파라미터를 보관할 b1 upload buffer를 생성한다.
		bool CreateParameterDataBuffers(const Dx12Device& sourceDevice);
		// 현재 frame의 pass 입력 descriptor를 실행 계획에 맞게 갱신한다.
		void UpdateInputDescriptors(const Dx12Device& sourceDevice, size_t frameIndex, size_t passIndex) const;
		// 모든 history target을 최초 사용 전에 0으로 지운다.
		void InitializeHistories(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext);
		// 공통 실행 계획의 모든 DX12 pipeline state를 생성한다.
		bool CreatePipelines(const Dx12Device& sourceDevice);
		// pass에 대응하는 shader-visible descriptor heap을 반환한다.
		ID3D12DescriptorHeap* ResolveInputDescriptorHeap(size_t frameIndex, size_t passIndex) const;
		// 입력 경로에 대응하는 DX12 resource와 SRV 형식을 반환한다.
		ID3D12Resource* ResolveInputResource(const PostProcessPassInputRoute& input, DXGI_FORMAT& format) const;
		// 출력 경로에 대응하는 DX12 target을 반환한다.
		Dx12PostProcessTarget* ResolveOutputTarget(const PostProcessPassRoute& route);
		// 선언 기반 effect target과 descriptor를 해제한다.
		void ResetEffectResources();
		
	public:
		// 현재 크기와 선택된 effect 선언에 맞는 DX12 후처리 target을 생성한다.
		bool InitializeTargets(const Dx12Device& sourceDevice, int width, int height);
		// 체크된 후처리 effect 선언으로 DX12 실행 리소스와 pipeline을 생성한다.
		bool Load(const Dx12Device& sourceDevice, const std::vector<const EffectRuntimeDefinition*>& effects);
		// DX12 후처리 장면 depth와 velocity 입력 패스를 시작한다.
		bool BeginSceneInputPass(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext, int width, int height) const;
		// DX12 후처리 장면 입력 패스를 종료하고 기록 성공 여부를 반환한다.
		bool EndSceneInputPass(ID3D12GraphicsCommandList* commandList,
			const Dx12CommandContext& commandContext) const;
		// 준비된 실행 계획으로 장면 색상을 swapchain back buffer에 그린다.
		bool Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
			const Dx12MsaaColorBuffer& msaaColorBuffer, const Dx12Device& sourceDevice,
			const Dx12CommandContext& commandContext, const Dx12SwapChain& swapChain,
			int width, int height, const PostProcessFrameData& frameData);
		// 생성한 DX12 후처리 리소스를 해제한다.
		void ResetResources() override;
	};
}
