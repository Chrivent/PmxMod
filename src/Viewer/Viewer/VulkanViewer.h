#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/VulkanDrawContext.h"
#include "Viewer/Command/VulkanCommandContext.h"
#include "Viewer/Command/VulkanUploadContext.h"
#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/RenderTarget/VulkanMsaaColorBuffer.h"
#include "Viewer/RenderTarget/VulkanMsaaDepthBuffer.h"
#include "Viewer/Pipeline/VulkanPipeline.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"
#include "Viewer/Synchronization/VulkanSyncObject.h"
#include "Viewer/PostProcess/VulkanPostProcess.h"
#include "Viewer/Texture/VulkanTextureCache.h"

#include <memory>

namespace Chrivent {
	// 공통 Viewer 계약을 Vulkan command buffer와 스왑체인 흐름으로 구현한다.
	class VulkanViewer final : public Viewer {
		VulkanDevice device;
		VulkanSwapChain swapChain;
		VulkanMsaaColorBuffer msaaColorBuffer;
		VulkanMsaaDepthBuffer msaaDepthBuffer;
		VulkanPipeline pipeline;
		VulkanCommandContext commandContext;
		VulkanSyncObject syncObject;
		VulkanTexture dummyTexture;
		bool postProcessSceneInputPassReady = false;
		VulkanDrawContext drawContext{ pipeline, commandContext };
		VulkanPostProcess postProcess;
		VulkanUploadContext uploadContext;
		VulkanTextureCache textureCache{ uploadContext };

		// swapchain 크기와 포맷에 의존하는 렌더링 리소스를 생성한다.
		GraphicsError::Result<void> CreateSwapChainResources();
		// swapchain 재생성 전에 의존 리소스를 역순으로 해제한다.
		void ResetSwapChainResources();

	protected:
		// 체크된 후처리 효과들을 검증한 뒤 현재 Vulkan 실행 체인과 교체한다.
		GraphicsError::Result<void> LoadPostProcessEffectsCore(
			PreparedPostProcessEffects preparedEffects) override;
		// Vulkan 후처리 장면 입력 패스 기록을 시작한다.
		GraphicsError::Result<void> BeginPostProcessSceneInputPassCore() override;
		// Vulkan 후처리 장면 입력 패스를 종료한다.
		GraphicsError::Result<void> EndPostProcessSceneInputPassCore() override;
		// Vulkan 디바이스, 스왑체인과 파이프라인 리소스를 초기화한다.
		GraphicsError::Result<void> SetupCore(const SceneShaderRuntimeContract& shaderContract) override;
		// Vulkan 스왑체인과 크기 의존 리소스를 재생성한다.
		GraphicsError::Result<void> ResizeCore() override;
		// Vulkan 프레임 명령 기록을 시작한다.
		GraphicsError::Result<FrameBeginState> BeginFrameCore() override;
		// Vulkan command buffer 제출과 Present 결과를 반환한다.
		GraphicsError::Result<FrameEndState> EndFrameCore() override;
		// Vulkan device에 제출된 작업이 끝날 때까지 기다린다.
		GraphicsError::Result<void> WaitIdleCore() override;
		// 초기 상태의 Vulkan 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstanceCore() override;

	public:
		VulkanViewer() : Viewer(GraphicsApi::Vulkan, false) {}
	};
}
