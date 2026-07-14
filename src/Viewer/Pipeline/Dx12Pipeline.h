#pragma once

#include "Viewer/Device/Dx12Device.h"
#include "Viewer/Shader/ShaderPackage.h"
#include <d3d12.h>
#include <filesystem>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 모델 렌더링에 필요한 루트 서명과 그래픽 파이프라인 상태를 관리한다.
	class Dx12Pipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> modelRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOnlyFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOnlyBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneVelocityFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneVelocityBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> edgeRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> edgePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> groundShadowRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> groundShadowPipelineState;
		
		// PMX 재질의 alpha blend 방식에 맞는 render target blend state를 채운다.
		static void ConfigureAlphaBlend(D3D12_RENDER_TARGET_BLEND_DESC& blendDesc);
		// 일반 메시와 엣지 패스에서 쓰는 depth stencil 기본값을 채운다.
		static void ConfigureDefaultDepthStencil(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc);
		// 모델 셰이더의 리소스 배치와 맞는 root signature를 생성한다.
		bool CreateModelRootSignature(const Dx12Device& sourceDevice);
		// 모델 렌더링용 graphics pipeline state를 생성한다.
		bool CreateModelPipelineStates(const Dx12Device& sourceDevice, const EffectPassDefinition& pass);
		// 포스트 프로세스용 depth-only graphics pipeline state를 생성한다.
		bool CreateDepthOnlyPipelineStates(const Dx12Device& sourceDevice, const EffectPassDefinition& pass);
		// 현재/이전 정점 위치를 RG16F 속도 타깃에 기록하는 pipeline state를 생성한다.
		bool CreateSceneVelocityPipelineStates(const Dx12Device& sourceDevice,
			const std::filesystem::path& shaderPath);
		// 엣지 셰이더의 상수 버퍼 배치와 맞는 root signature를 생성한다.
		bool CreateEdgeRootSignature(const Dx12Device& sourceDevice);
		// 엣지 렌더링용 graphics pipeline state를 생성한다.
		bool CreateEdgePipelineState(const Dx12Device& sourceDevice, const EffectPassDefinition& pass);
		// 지면 그림자 셰이더의 상수 버퍼 배치와 맞는 root signature를 생성한다.
		bool CreateGroundShadowRootSignature(const Dx12Device& sourceDevice);
		// 지면 그림자 렌더링용 graphics pipeline state를 생성한다.
		bool CreateGroundShadowPipelineState(const Dx12Device& sourceDevice, const EffectPassDefinition& pass);
		
	public:
		// DX12 모델 렌더링에 필요한 root signature와 pipeline state를 초기화한다.
		bool Initialize(const Dx12Device& sourceDevice, const BuiltInShaderPasses& passes,
			const std::filesystem::path& sceneVelocityShaderPath);
		// material의 양면 렌더링 여부에 맞는 model pipeline을 command list에 바인딩한다.
		void BindModel(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// material의 양면 렌더링 여부에 맞는 depth-only pipeline을 command list에 바인딩한다.
		void BindDepthOnly(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// material의 양면 렌더링 여부에 맞는 장면 속도 pipeline을 바인딩한다.
		void BindSceneVelocity(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// 엣지 렌더링용 pipeline을 command list에 바인딩한다.
		void BindEdge(ID3D12GraphicsCommandList* commandList) const;
		// 지면 그림자 렌더링용 pipeline을 command list에 바인딩한다.
		void BindGroundShadow(ID3D12GraphicsCommandList* commandList) const;
		// 생성한 DX12 pipeline 리소스를 해제한다.
		void Reset();
	};
}
