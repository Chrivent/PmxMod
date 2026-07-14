#pragma once

#include "Viewer/Shader/Dx11Shader.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <d3d11.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	struct PostProcessFrameData;

	struct Dx11PostProcessResource {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> textures[2];
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViews[2];
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceViews[2];
		size_t historyIndex = 0;
		bool historyInitialized = false;
	};

	class Dx11PostProcess : public PostProcess {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColor;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> velocity;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> velocityRenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocityView;
		Microsoft::WRL::ComPtr<ID3D11Buffer> frameDataBuffer;
		std::vector<Dx11PostProcessResource> resources;
		std::vector<Dx11PostProcessShader> postProcessShaders;
		int targetWidth = 0;
		int targetHeight = 0;

		// 화면 크기에 맞는 DX11 viewport를 immediate context에 설정한다.
		static void ApplyViewport(ID3D11DeviceContext* context, int width, int height);
		// 패키지가 선언한 transient/history texture view를 생성한다.
		bool CreateEffectResources(ID3D11Device* device);
		// 초기화가 필요한 모든 history texture를 0으로 지운다.
		void InitializeHistories(ID3D11DeviceContext* context);
		// pass 입력 경로에 대응하는 DX11 SRV를 반환한다.
		ID3D11ShaderResourceView* ResolveInputView(const PostProcessPassInputRoute& input) const;
		// pass 출력 경로에 대응하는 DX11 RTV를 반환한다.
		ID3D11RenderTargetView* ResolveOutputView(
			const PostProcessPassRoute& route, ID3D11RenderTargetView* backBufferView) const;
		// pass 출력 크기를 계산한다.
		void ResolveOutputExtent(const PostProcessPassRoute& route, int& width, int& height) const;
		// history 출력 pass가 끝난 뒤 read/write 인덱스를 전환한다.
		void AdvanceHistory(const PostProcessPassRoute& route);
		// 패키지가 선언한 DX11 effect texture를 해제한다.
		void ResetEffectResources();
		// DX11 후처리 셰이더만 해제한다.
		void ResetShaders();

	public:
		// 화면 크기에 맞는 DX11 후처리 장면 색상/depth와 선언된 effect target을 생성한다.
		bool InitializeTargets(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height);
		// MSAA 장면 색상을 소유한 단일 샘플 입력 texture로 resolve한다.
		void ResolveSceneColor(ID3D11DeviceContext* context, ID3D11Texture2D* source, UINT sampleCount) const;
		// 체크된 후처리 effect 선언으로 DX11 실행 리소스와 shader chain을 생성한다.
		bool Load(ID3D11Device* device, const std::vector<const EffectDefinition*>& effects);
		// DX11 후처리 셰이더와 선택 effect 목록만 해제한다.
		void ClearShaders();
		// 후처리용 depth-only pass를 시작한다.
		bool BeginDepthPass(ID3D11DeviceContext* context, ID3D11DepthStencilState* depthStencilState,
			int width, int height) const;
		// 후처리용 depth-only pass를 종료한다.
		static void EndDepthPass(ID3D11DeviceContext* context);
		// 준비된 실행 계획으로 화면 색상을 swapchain back buffer에 그린다.
		void Draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* backBufferView,
			ID3D11RasterizerState* rasterizerState, ID3D11SamplerState* sampler,
			int width, int height, const PostProcessFrameData& frameData);
		// 다음 후처리 프레임에서 모든 DX11 history를 0으로 초기화한다.
		void ResetHistory() override;
		// 생성한 DX11 후처리 target을 해제한다.
		void ResetTargets();
		// 생성한 DX11 후처리 상태를 해제한다.
		void Reset() override;
	};
}
