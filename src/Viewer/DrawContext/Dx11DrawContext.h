#pragma once

#include "Viewer/Shader/Dx11Shader.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 device, immediate context와 swapchain을 한 단위로 보관한다.
	struct Dx11DeviceResources {
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
	};

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
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> frontFaceRs;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> bothFaceRs;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> edgeRs;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> gsRs;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> gsDss;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> defaultDss;
	};

	// texture가 없는 D3D11 재질에 바인딩할 기본 흰색 texture를 보관한다.
	struct Dx11DummyTexture {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
	};

	// D3D11 Drawer와 Instance에 장면 그리기에 필요한 API 리소스만 노출한다.
	class Dx11DrawContext {
		const Dx11DeviceResources& deviceResources;
		const Dx11ShaderSet& shaders;
		const Dx11PipelineStates& pipelineStates;
		const Dx11DummyTexture& dummyTexture;

	public:
		Dx11DrawContext(const Dx11DeviceResources& sourceDeviceResources, const Dx11ShaderSet& sourceShaders,
			const Dx11PipelineStates& sourcePipelineStates, const Dx11DummyTexture& sourceDummyTexture)
			: deviceResources(sourceDeviceResources), shaders(sourceShaders),
			pipelineStates(sourcePipelineStates), dummyTexture(sourceDummyTexture) {}

		const Dx11DeviceResources& GetDeviceResources() const { return deviceResources; }
		const Dx11ShaderSet& GetShaders() const { return shaders; }
		const Dx11PipelineStates& GetPipelineStates() const { return pipelineStates; }
		const Dx11DummyTexture& GetDummyTexture() const { return dummyTexture; }

		// 현재 출력 크기에 맞는 viewport를 immediate context에 적용한다.
		static void ApplyViewport(ID3D11DeviceContext* context, int width, int height);
	};
}
