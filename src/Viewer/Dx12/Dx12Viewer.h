#pragma once

#include "Viewer/Viewer.h"
#include "Viewer/Dx12/Dx12TextureCache.h"
#include "Viewer/Dx12/Helper/Dx12CommandContext.h"
#include "Viewer/Dx12/Helper/Dx12DepthBuffer.h"
#include "Viewer/Dx12/Helper/Dx12Device.h"
#include "Viewer/Dx12/Helper/Dx12MsaaColorBuffer.h"
#include "Viewer/Dx12/Helper/Dx12Pipeline.h"
#include "Viewer/Dx12/Helper/Dx12PostProcessTarget.h"
#include "Viewer/Dx12/Helper/Dx12SwapChain.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace Chrivent {
	struct Dx12Material : ViewerMaterial {
		Dx12Texture texture{};
		Dx12Texture sphereTexture{};
		Dx12Texture toonTexture{};
		D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle{};

		explicit Dx12Material(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};

	class Dx12Viewer : public Viewer {
	public:
		std::shared_ptr<Dx12Device> device;
		Dx12SwapChain swapChain;
		Dx12MsaaColorBuffer msaaColorBuffer;
		std::vector<Dx12PostProcessTarget> postProcessTargets;
		Dx12DepthBuffer depthBuffer;
		Dx12CommandContext commandContext;
		Dx12Pipeline pipeline;
		Dx12TextureCache textureCache;
		std::shared_ptr<Dx12Texture> dummyTexture;
		bool frameReady = false;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		UINT frameIndex = 0;

	private:
		// 현재 back buffer를 render target 상태로 전환한다.
		void PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const;
		// MSAA color/depth target을 바인딩하고 프레임 시작 clear를 수행한다.
		void ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const;
		// 현재 화면 크기에 맞는 viewport와 scissor rect를 명령 목록에 적용한다.
		void ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList) const;
		// MSAA color buffer의 결과를 현재 back buffer로 옮기고 present 상태로 되돌린다.
		void ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer, ID3D12Resource* msaaColor) const;
		// MSAA 장면을 후처리 입력으로 resolve하고 선택된 효과로 back buffer에 그린다.
		void DrawPostProcess(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer, ID3D12Resource* msaaColor) const;

	public:
		Dx12Viewer();
		~Dx12Viewer() override;

		bool IsFrameReady() const { return frameReady; }

		// DX12 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
		void ConfigureGlfwHints() override;
		// DX12 렌더러 리소스를 초기화한다.
		bool Setup() override;
		// 창 크기에 맞춰 DX12 스왑체인과 렌더 타깃을 재생성한다.
		bool Resize() override;
		// DX12 프레임 렌더링을 시작한다.
		void BeginFrame() override;
		// DX12 프레임을 제출하고 화면에 표시한다.
		bool EndFrame() override;
		// DX12 command queue에 제출된 작업이 끝날 때까지 기다린다.
		void WaitIdle() override;
		// 체크된 포스트 프로세스 효과들을 DX12 ping-pong 체인으로 컴파일한다.
		bool LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) override;
		// DX12 후처리 pipeline들을 해제한다.
		void ClearPostProcessEffects() override;
		// DX12 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstance() const override;
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX12 텍스처로 반환한다.
		Dx12Texture LoadTexture(const std::filesystem::path& texturePath);
		// material의 양면 렌더링 여부에 맞는 DX12 model pipeline state를 바인딩한다.
		void BindModelPipeline(bool bothFace) const;
		// DX12 엣지 렌더링용 root signature와 pipeline state를 바인딩한다.
		void BindEdgePipeline() const;
		// DX12 지면 그림자 렌더링용 root signature와 pipeline state를 바인딩한다.
		void BindGroundShadowPipeline() const;
	};
}
