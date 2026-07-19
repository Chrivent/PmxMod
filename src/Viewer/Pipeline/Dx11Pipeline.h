#pragma once

#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/Dx11Shader.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 장면 셰이더와 고정 pipeline state의 생성 및 소유를 담당한다.
	class Dx11Pipeline {
		// D3D11 기본 렌더링 패스별 셰이더 프로그램을 보관한다.
		struct ShaderSet {
			Dx11ModelShader model;
			Dx11EdgeShader edge;
			Dx11GroundShadowShader groundShadow;
			Dx11SceneDepthShader sceneDepth;
			Dx11SceneVelocityShader sceneVelocity;
		};

		// D3D11 기본 렌더링에 사용하는 sampler와 고정 파이프라인 상태를 보관한다.
		struct PipelineStates {
			Microsoft::WRL::ComPtr<ID3D11SamplerState> textureSampler;
			Microsoft::WRL::ComPtr<ID3D11SamplerState> toonTextureSampler;
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
			Microsoft::WRL::ComPtr<ID3D11BlendState> groundShadowBlendState;
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> frontFaceRs;
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> bothFaceRs;
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> edgeRs;
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> gsRs;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> gsDss;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> defaultDss;
		};

		ShaderSet shaders;
		PipelineStates states;

		// 장면 ABI 패스에 대응하는 D3D11 셰이더를 생성한다.
		GraphicsResult<void> CreateShaders(ID3D11Device* device,
			const BuiltInShaderPasses& builtInPasses,
			const SceneInputShaderPasses& sceneInputPasses);
		// sampler, blend, rasterizer와 depth-stencil 상태를 생성한다.
		GraphicsResult<void> CreateStates(ID3D11Device* device);

	public:
		ID3D11SamplerState* GetTextureSampler() const { return states.textureSampler.Get(); }
		ID3D11SamplerState* GetToonTextureSampler() const { return states.toonTextureSampler.Get(); }
		ID3D11RasterizerState* GetBothFaceRasterizerState() const { return states.bothFaceRs.Get(); }
		ID3D11DepthStencilState* GetDefaultDepthStencilState() const { return states.defaultDss.Get(); }

		// 장면 ABI 계약으로 D3D11 셰이더와 고정 pipeline state를 생성한다.
		GraphicsResult<void> Initialize(ID3D11Device* device,
			const SceneShaderRuntimeContract& shaderContract);
		// 기본 장면 alpha blend 상태를 immediate context에 적용한다.
		void BindDefaultBlendState(ID3D11DeviceContext* context) const;
		// 재질의 양면 여부에 맞는 rasterizer state를 반환한다.
		ID3D11RasterizerState* ResolveModelRasterizerState(bool bothFace) const;
		// 모델 표면 셰이더와 고정 pipeline state를 바인딩한다.
		void BindModel(ID3D11DeviceContext* context) const;
		// 엣지 셰이더와 고정 pipeline state를 바인딩한다.
		void BindEdge(ID3D11DeviceContext* context) const;
		// 지면 그림자 셰이더와 고정 pipeline state를 바인딩한다.
		void BindGroundShadow(ID3D11DeviceContext* context) const;
		// depth 또는 velocity 장면 입력 셰이더를 바인딩한다.
		void BindSceneInput(ID3D11DeviceContext* context, bool velocity) const;
	};
}
