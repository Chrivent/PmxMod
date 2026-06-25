#pragma once

#include "Viewer/Dx12/Helper/Dx12Device.h"
#include "Viewer/Shader/ShaderPackage.h"

#include <d3d12.h>
#include <filesystem>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12Pipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> modelRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOnlyFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOnlyBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> edgeRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> edgePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> groundShadowRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> groundShadowPipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> postProcessRootSignature;
		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> postProcessPipelineStates;

		// root signature 직렬화와 생성 실패 로그 처리를 공통으로 수행한다.
		static bool CreateRootSignature(
			const Dx12Device& sourceDevice,
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc,
			Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature);
		// PMX 재질의 alpha blend 방식에 맞는 render target blend state를 채운다.
		static void ConfigureAlphaBlend(D3D12_RENDER_TARGET_BLEND_DESC& blendDesc);
		// 공통 rasterizer 기본값을 채우고 패스별 cull mode만 반영한다.
		static void ConfigureRasterizer(D3D12_RASTERIZER_DESC& rasterizerDesc, D3D12_CULL_MODE cullMode);
		// 일반 메시와 엣지 패스에서 쓰는 depth stencil 기본값을 채운다.
		static void ConfigureDefaultDepthStencil(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc);
		// 디바이스 shader model에 맞춰 DXC 또는 레거시 컴파일러로 셰이더를 만든다.
		static bool CompileShader(const Dx12Device& sourceDevice, const std::filesystem::path& file,
			const std::string& entry, bool vertexShader, std::vector<uint8_t>& bytecode, std::string& error);
		
		// 모델 셰이더의 리소스 배치와 맞는 root signature를 생성한다.
		bool CreateModelRootSignature(const Dx12Device& sourceDevice);
		// 모델 렌더링용 graphics pipeline state를 생성한다.
		bool CreateModelPipelineStates(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// 포스트 프로세스용 depth-only graphics pipeline state를 생성한다.
		bool CreateDepthOnlyPipelineStates(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// 엣지 셰이더의 상수 버퍼 배치와 맞는 root signature를 생성한다.
		bool CreateEdgeRootSignature(const Dx12Device& sourceDevice);
		// 엣지 렌더링용 graphics pipeline state를 생성한다.
		bool CreateEdgePipelineState(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// 지면 그림자 셰이더의 상수 버퍼 배치와 맞는 root signature를 생성한다.
		bool CreateGroundShadowRootSignature(const Dx12Device& sourceDevice);
		// 지면 그림자 렌더링용 graphics pipeline state를 생성한다.
		bool CreateGroundShadowPipelineState(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// 후처리 입력 SRV와 sampler를 노출하는 root signature를 생성한다.
		bool CreatePostProcessRootSignature(const Dx12Device& sourceDevice);

	public:
		// DX12 모델 렌더링에 필요한 root signature와 pipeline state를 초기화한다.
		bool Initialize(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// material의 양면 렌더링 여부에 맞는 model pipeline을 command list에 바인딩한다.
		void BindModel(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// material의 양면 렌더링 여부에 맞는 depth-only pipeline을 command list에 바인딩한다.
		void BindDepthOnly(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// 엣지 렌더링용 pipeline을 command list에 바인딩한다.
		void BindEdge(ID3D12GraphicsCommandList* commandList) const;
		// 지면 그림자 렌더링용 pipeline을 command list에 바인딩한다.
		void BindGroundShadow(ID3D12GraphicsCommandList* commandList) const;
		// 체크된 후처리 HLSL 목록으로 단일 샘플 graphics pipeline 체인을 생성한다.
		bool LoadPostProcessEffects(const Dx12Device& sourceDevice, const std::vector<const EffectDefinition*>& effects);
		// 후처리 pipeline과 장면 색상 SRV를 command list에 바인딩한다.
		void BindPostProcess(ID3D12GraphicsCommandList* commandList, size_t passIndex, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle) const;
		// 선택된 후처리 pipeline이 준비됐는지 반환한다.
		bool HasPostProcessEffect() const { return !postProcessPipelineStates.empty(); }
		// 선택된 후처리 pipeline 개수를 반환한다.
		size_t GetPostProcessPassCount() const { return postProcessPipelineStates.size(); }
		// 선택된 후처리 pipeline 리소스만 해제한다.
		void ClearPostProcessEffects();
		// 생성한 DX12 pipeline 리소스를 해제한다.
		void Reset();
	};
}
