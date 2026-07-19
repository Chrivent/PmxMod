#pragma once

#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/Dx11Shader.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <d3d11.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	struct PostProcessFrameData;

	// 공통 실행 계획을 D3D11 렌더 타깃과 셰이더 패스로 실행한다.
	class Dx11PostProcess : public PostProcess {
		// D3D11 후처리 리소스의 ping-pong texture와 view를 보관한다.
		struct Resource {
			Microsoft::WRL::ComPtr<ID3D11Texture2D> textures[2];
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViews[2];
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceViews[2];
		};

		Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColor;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> velocity;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> velocityRenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> velocityView;
		Microsoft::WRL::ComPtr<ID3D11Buffer> frameDataBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> parameterDataBuffer;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> fullscreenRasterizerState;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> linearClampSampler;
		std::vector<Resource> resources;
		std::vector<Dx11ShaderProgram> postProcessPrograms;
		int targetWidth = 0;
		int targetHeight = 0;

		// 동적 상수 버퍼를 새 저장소로 매핑하고 후처리 입력 데이터를 기록한다.
		static GraphicsResult<void> WriteConstantBuffer(ID3D11DeviceContext* context,
			ID3D11Buffer* buffer, const void* data, size_t size, const char* operation);
		// 패키지가 선언한 transient/history texture view를 생성한다.
		GraphicsResult<void> CreateEffectResources(ID3D11Device* device);
		// 초기화가 필요한 모든 history texture를 0으로 지운다.
		void InitializeHistories(ID3D11DeviceContext* context);
		// pass 입력 경로에 대응하는 DX11 SRV를 반환한다.
		ID3D11ShaderResourceView* ResolveInputView(const PostProcessPassInputRoute& input) const;
		// pass 출력 경로에 대응하는 DX11 RTV를 반환한다.
		ID3D11RenderTargetView* ResolveOutputView(
			const PostProcessPassRoute& route, ID3D11RenderTargetView* backBufferView) const;
		// 패키지가 선언한 DX11 effect texture를 해제한다.
		void ResetEffectResources();
		// DX11 후처리 프로그램만 해제한다.
		void ResetPrograms();
		// 현재 실행 계획의 DX11 후처리 프로그램을 생성한다.
		GraphicsResult<void> CreatePrograms(ID3D11Device* device);
		// 풀스크린 후처리 전용 rasterizer와 sampler state를 생성한다.
		GraphicsResult<void> CreateStates(ID3D11Device* device);
		// 검증을 마친 다른 DX11 후처리 객체와 GPU 리소스를 교환한다.
		void SwapResources(Dx11PostProcess& other) noexcept;
		
	public:
		// 화면 크기에 맞는 DX11 후처리 장면 색상/depth와 선언된 effect target을 생성한다.
		GraphicsResult<void> InitializeTargets(ID3D11Device* device, int width, int height);
		// 효과 선택 변경에 맞춰 DX11 타깃과 shader chain의 전체 생명주기를 갱신한다.
		GraphicsResult<void> Configure(ID3D11Device* device, int width, int height,
			const std::vector<const EffectRuntimeDefinition*>& effects);
		// DX11 후처리 장면 depth와 velocity 입력 패스를 시작한다.
		GraphicsResult<void> BeginSceneInputPass(ID3D11DeviceContext* context,
			ID3D11DepthStencilState* depthStencilState, int width, int height) const;
		// DX11 후처리 장면 입력 패스를 종료한다.
		static GraphicsResult<void> EndSceneInputPass(ID3D11DeviceContext* context);
		// 준비된 실행 계획으로 화면 색상을 swapchain back buffer에 그린다.
		GraphicsResult<void> Draw(ID3D11DeviceContext* context, ID3D11Texture2D* sceneSource,
			UINT sampleCount, ID3D11RenderTargetView* backBufferView, int width, int height,
			const PostProcessFrameData& frameData);
		// 생성한 DX11 후처리 target을 해제한다.
		void ResetTargets();
		// 생성한 DX11 후처리 상태를 해제한다.
		void ResetResources() override;
	};
}
