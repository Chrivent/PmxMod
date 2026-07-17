#pragma once

#include "Viewer/Shader/Dx11Shader.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 기본 렌더링 패스별 셰이더 프로그램을 보관한다.
	struct Dx11ShaderSet {
		Dx11ModelShader model;
		Dx11EdgeShader edge;
		Dx11GroundShadowShader groundShadow;
		Dx11SceneDepthShader sceneDepth;
		Dx11SceneVelocityShader sceneVelocity;
	};

	// D3D11 기본 렌더링에 사용하는 sampler와 고정 파이프라인 상태를 보관한다.
	struct Dx11PipelineStates {
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

	// D3D11 장면 셰이더와 고정 pipeline state의 생성 및 소유를 담당한다.
	class Dx11Pipeline {
		Dx11ShaderSet shaders;
		Dx11PipelineStates states;

		// 장면 ABI 패스에 대응하는 D3D11 셰이더를 생성한다.
		bool CreateShaders(ID3D11Device* device, const BuiltInShaderPasses& builtInPasses,
			const SceneInputShaderPasses& sceneInputPasses);
		// sampler, blend, rasterizer와 depth-stencil 상태를 생성한다.
		bool CreateStates(ID3D11Device* device);

	public:
		const Dx11ShaderSet& GetShaders() const { return shaders; }
		const Dx11PipelineStates& GetStates() const { return states; }

		// 장면 ABI 계약으로 D3D11 셰이더와 고정 pipeline state를 생성한다.
		bool Initialize(ID3D11Device* device, const BuiltInShaderPasses& builtInPasses,
			const SceneInputShaderPasses& sceneInputPasses);
	};
}
