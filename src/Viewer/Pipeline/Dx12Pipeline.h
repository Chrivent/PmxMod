#pragma once

#include "Viewer/Device/Dx12Device.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"
#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 모델 렌더링에 필요한 루트 서명과 그래픽 파이프라인 상태를 관리한다.
	class Dx12Pipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> modelRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneDepthFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneDepthBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneVelocityFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneVelocityBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> simplePassRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> edgePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> groundShadowPipelineState;
		
		// PMX 재질의 alpha blend 방식에 맞는 render target blend state를 채운다.
		static void ConfigureAlphaBlend(D3D12_RENDER_TARGET_BLEND_DESC& blendDesc);
		// 일반 메시와 엣지 패스에서 쓰는 depth stencil 기본값을 채운다.
		static void ConfigureDefaultDepthStencil(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc);
		// 모델 셰이더의 리소스 배치와 맞는 root signature를 생성한다.
		GraphicsResult<void> CreateModelRootSignature(const Dx12Device& sourceDevice);
		// 모델 렌더링용 graphics pipeline state를 생성한다.
		GraphicsResult<void> CreateModelPipelineStates(
			const Dx12Device& sourceDevice, const ShaderProgramDefinition& program);
		// 공통 장면 depth 입력용 graphics pipeline state를 생성한다.
		GraphicsResult<void> CreateSceneDepthPipelineStates(
			const Dx12Device& sourceDevice, const ShaderProgramDefinition& program);
		// 현재/이전 정점 위치를 RG16F velocity 타깃에 기록하는 pipeline state를 생성한다.
		GraphicsResult<void> CreateSceneVelocityPipelineStates(
			const Dx12Device& sourceDevice, const ShaderProgramDefinition& program);
		// 엣지와 지면 그림자가 공유하는 상수 버퍼 전용 root signature를 생성한다.
		GraphicsResult<void> CreateSimplePassRootSignature(const Dx12Device& sourceDevice);
		// 엣지 렌더링용 graphics pipeline state를 생성한다.
		GraphicsResult<void> CreateEdgePipelineState(
			const Dx12Device& sourceDevice, const ShaderProgramDefinition& program);
		// 지면 그림자 렌더링용 graphics pipeline state를 생성한다.
		GraphicsResult<void> CreateGroundShadowPipelineState(
			const Dx12Device& sourceDevice, const ShaderProgramDefinition& program);
		// 검증된 다른 DX12 파이프라인과 GPU 리소스를 교환한다.
		void SwapResources(Dx12Pipeline& other) noexcept;
		
	public:
		// DX12 모델 렌더링에 필요한 root signature와 pipeline state를 초기화한다.
		GraphicsResult<void> Initialize(const Dx12Device& sourceDevice,
			const SceneShaderRuntimeContract& shaderContract);
		// 모델 계열 pipeline에서 공유하는 root signature를 command list에 바인딩한다.
		void BindModelRootSignature(ID3D12GraphicsCommandList* commandList) const;
		// material의 양면 렌더링 여부에 맞는 model pipeline state를 command list에 바인딩한다.
		void BindModelPipelineState(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// material의 양면 렌더링 여부에 맞는 장면 depth pipeline state를 command list에 바인딩한다.
		void BindSceneDepthPipelineState(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// material의 양면 렌더링 여부에 맞는 장면 속도 pipeline state를 바인딩한다.
		void BindSceneVelocityPipelineState(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// 엣지 렌더링용 pipeline을 command list에 바인딩한다.
		void BindEdge(ID3D12GraphicsCommandList* commandList) const;
		// 지면 그림자 렌더링용 pipeline을 command list에 바인딩한다.
		void BindGroundShadow(ID3D12GraphicsCommandList* commandList) const;
		// 생성한 DX12 pipeline 리소스를 해제한다.
		void Reset();
	};
}
