#pragma once

#include "Viewer/Dx11/Helper/Dx11Shader.h"
#include "Viewer/PostProcess.h"

#include <d3d11.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	class Dx11PostProcess : public PostProcess {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColor;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pingPongColor[2];
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pingPongColorView[2];
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pingPongColorResourceView[2];
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> focusHistory[2];
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> focusHistoryView[2];
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> focusHistoryResourceView[2];
		Dx11PostProcessShader focusHistoryShader;
		std::vector<Dx11PostProcessShader> postProcessShaders;
		int focusHistoryIndex = 0;
		bool focusHistoryEnabled = false;
		bool focusHistoryInitialized = false;

		// 화면 크기에 맞는 DX11 viewport를 immediate context에 설정한다.
		static void ApplyViewport(ID3D11DeviceContext* context, int width, int height);
		// 초기화 요청 뒤 초점 히스토리 target들을 0으로 지운다.
		void InitializeFocusHistory(ID3D11DeviceContext* context);
		// DOF용 자동 초점 히스토리 텍스처를 갱신한다.
		void UpdateFocusHistory(ID3D11DeviceContext* context, int width, int height);
		// DX11 후처리 셰이더와 선택 effect 상태만 해제한다.
		void ResetShaders();

	public:
		Dx11PostProcess() = default;
		~Dx11PostProcess() override = default;

		Dx11PostProcess(const Dx11PostProcess&) = delete;
		Dx11PostProcess& operator=(const Dx11PostProcess&) = delete;
		Dx11PostProcess(Dx11PostProcess&&) = delete;
		Dx11PostProcess& operator=(Dx11PostProcess&&) = delete;

		// 화면 크기에 맞는 DX11 후처리 색상/depth/focus history target을 생성한다.
		bool InitializeTargets(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height);
		// MSAA 장면 색상을 소유한 단일 샘플 입력 텍스처로 resolve한다.
		void ResolveSceneColor(ID3D11DeviceContext* context, ID3D11Texture2D* source, UINT sampleCount) const;
		// 체크된 후처리 effect 목록으로 DX11 fullscreen shader chain을 생성한다.
		bool Load(ID3D11Device* device, const std::vector<const EffectDefinition*>& effects);
		// DX11 후처리 셰이더와 선택 effect 목록만 해제한다.
		void ClearShaders();
		// 후처리용 depth-only pass를 시작한다.
		bool BeginDepthPass(ID3D11DeviceContext* context, ID3D11DepthStencilState* depthStencilState,
			int width, int height) const;
		// 후처리용 depth-only pass를 종료한다.
		static void EndDepthPass(ID3D11DeviceContext* context);
		// 준비된 후처리 셰이더로 화면 색상을 스왑체인 back buffer에 그린다.
		void Draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* backBufferView,
			ID3D11RasterizerState* rasterizerState, ID3D11SamplerState* sampler, int width, int height);
		// 다음 후처리 프레임에서 DX11 초점 히스토리를 0으로 초기화한다.
		void ResetFocusHistory() override;
		// 생성한 DX11 후처리 target을 해제한다.
		void ResetTargets();
		// 생성한 DX11 후처리 셰이더 상태를 해제한다.
		void Reset() override;
	};
}
