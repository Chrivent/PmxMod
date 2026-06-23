#pragma once

#include "Dx12Device.h"

#include <d3d12.h>
#include <filesystem>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12Pipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> modelRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelFrontFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> modelBothFacePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> edgeRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> edgePipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> groundShadowRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> groundShadowPipelineState;

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
		
		// 모델 셰이더의 리소스 배치와 맞는 root signature를 생성한다.
		bool CreateModelRootSignature(const Dx12Device& sourceDevice);
		// 모델 렌더링용 graphics pipeline state를 생성한다.
		bool CreateModelPipelineStates(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// 엣지 셰이더의 상수 버퍼 배치와 맞는 root signature를 생성한다.
		bool CreateEdgeRootSignature(const Dx12Device& sourceDevice);
		// 엣지 렌더링용 graphics pipeline state를 생성한다.
		bool CreateEdgePipelineState(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// 지면 그림자 셰이더의 상수 버퍼 배치와 맞는 root signature를 생성한다.
		bool CreateGroundShadowRootSignature(const Dx12Device& sourceDevice);
		// 지면 그림자 렌더링용 graphics pipeline state를 생성한다.
		bool CreateGroundShadowPipelineState(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);

	public:
		// DX12 모델 렌더링에 필요한 root signature와 pipeline state를 초기화한다.
		bool Initialize(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir);
		// material의 양면 렌더링 여부에 맞는 model pipeline을 command list에 바인딩한다.
		void BindModel(ID3D12GraphicsCommandList* commandList, bool bothFace) const;
		// 엣지 렌더링용 pipeline을 command list에 바인딩한다.
		void BindEdge(ID3D12GraphicsCommandList* commandList) const;
		// 지면 그림자 렌더링용 pipeline을 command list에 바인딩한다.
		void BindGroundShadow(ID3D12GraphicsCommandList* commandList) const;
		// 생성한 DX12 pipeline 리소스를 해제한다.
		void Destroy();
	};
}
