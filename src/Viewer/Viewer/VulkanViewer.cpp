#include "Viewer/Viewer/VulkanViewer.h"

#include "Viewer/Instance/VulkanInstance.h"

namespace Chrivent {
	bool VulkanViewer::CreateSwapChainResources() {
		if (!msaaColorBuffer.Initialize(device, swapChain))
			return false;
		if (!msaaDepthBuffer.Initialize(device, swapChain))
			return false;
		if (postProcess.HasEffects()) {
			if (!postProcess.InitializeTargets(device, swapChain, msaaDepthBuffer.format))
				return false;
		}
		return commandContext.Initialize(device, swapChain);
	}

	void VulkanViewer::ResetSwapChainResources() {
		commandContext.Reset();
		postProcess.ResetResources();
		msaaColorBuffer.Reset();
		msaaDepthBuffer.Reset();
	}

	GraphicsResult<void> VulkanViewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
		BindPostProcess(postProcess);
		if (!device.Initialize(window))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InitializationFailed,
				"device 초기화", "Vulkan device를 만들지 못했습니다"));
		if (!swapChain.Initialize(device, window))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"swap chain 초기화", "Vulkan swap chain을 만들지 못했습니다"));
		if (!CreateSwapChainResources())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"swap chain 리소스 초기화", "Vulkan 프레임 리소스를 만들지 못했습니다"));
		if (!pipeline.Initialize(device, swapChain.GetImageFormat(),
			msaaDepthBuffer.format, shaderContract))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"rendering pipeline 초기화", "Vulkan pipeline을 만들지 못했습니다"));
		dummyTexture = textureCache.CreateWhiteTexture(device);
		if (dummyTexture.image == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"dummy texture 생성", "fallback texture를 만들지 못했습니다"));
		if (!syncObject.Initialize(device, swapChain.GetImageCount()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"프레임 동기화 초기화", "Vulkan 동기화 객체를 만들지 못했습니다"));
		return {};
	}

	GraphicsResult<void> VulkanViewer::ResizeCore() {
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		ResetSwapChainResources();
		if (!swapChain.Recreate(device, window))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"swap chain 크기 변경", "Vulkan swap chain을 다시 만들지 못했습니다"));
		if (!CreateSwapChainResources())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"swap chain 리소스 크기 변경", "Vulkan 프레임 리소스를 다시 만들지 못했습니다"));
		if (!pipeline.IsCompatible(swapChain.GetImageFormat(),
			msaaDepthBuffer.format, device.msaaSampleCount))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ContractViolation,
				"크기 변경 후 pipeline 검증", "Vulkan pipeline이 새 swap chain과 호환되지 않습니다"));
		if (!syncObject.ResetImageTracking(swapChain.GetImageCount()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"프레임 동기화 크기 변경", "Vulkan 이미지 추적 상태를 초기화하지 못했습니다"));
		return {};
	}

	GraphicsResult<FrameBeginState> VulkanViewer::BeginFrameCore() {
		drawContext.EndFrame();
		postProcessSceneInputPassReady = false;
		const size_t frameIndex = syncObject.GetCurrentFrameIndex();
		if (!syncObject.WaitForCurrentFrame())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"현재 프레임 대기", "Vulkan 프레임 fence 대기에 실패했습니다"));
		uint32_t currentImageIndex = 0;
		const VkResult acquireResult = vkAcquireNextImageKHR(device.device, swapChain.GetSwapChain(), UINT64_MAX,
			syncObject.GetImageAvailableSemaphore(), VK_NULL_HANDLE, &currentImageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			const auto recreateResult = RecreateFromFramebuffer();
			if (!recreateResult)
				return std::unexpected(recreateResult.error());
			return FrameBeginState::Skipped;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::PresentationFailed,
				"swap chain 이미지 획득", "Vulkan이 다음 이미지를 획득하지 못했습니다", acquireResult, true));
		if (!syncObject.WaitForImage(currentImageIndex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"swap chain 이미지 대기", "Vulkan 이미지 fence 대기에 실패했습니다"));
		auto& commandBuffer = commandContext.GetCommandBuffer();
		const VkResult resetResult = vkResetCommandBuffer(commandBuffer.TryGetCommandBuffer(currentImageIndex), 0);
		if (resetResult != VK_SUCCESS)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"command buffer 초기화", "Vulkan command buffer를 초기화하지 못했습니다", resetResult, true));
		const VkImage resolveImage = postProcess.HasEffects()
			? postProcess.TryGetSceneImage(currentImageIndex)
			: swapChain.GetImage(currentImageIndex);
		const VkImageView resolveImageView = postProcess.HasEffects()
			? postProcess.TryGetSceneImageView(currentImageIndex)
			: swapChain.GetImageView(currentImageIndex);
		if (!commandBuffer.BeginRecord(currentImageIndex,
			msaaColorBuffer.GetImage(), msaaColorBuffer.imageView, resolveImage, resolveImageView,
			msaaDepthBuffer.GetImage(), msaaDepthBuffer.imageView,
			VulkanMsaaDepthBuffer::HasStencilComponent(msaaDepthBuffer.format),
			device.msaaSampleCount, swapChain.GetExtent(), clearColor))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"command buffer 시작", "Vulkan 프레임 command buffer가 기록을 시작하지 못했습니다"));
		drawContext.BeginFrame(currentImageIndex, frameIndex);
		return FrameBeginState::Ready;
	}

	GraphicsResult<FrameEndState> VulkanViewer::EndFrameCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"프레임 종료", "Vulkan draw context가 준비되지 않았습니다"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		const bool sceneInputPassReady = postProcessSceneInputPassReady;
		drawContext.EndFrame();
		postProcessSceneInputPassReady = false;
		const auto& commandBuffer = commandContext.GetCommandBuffer();
		if (postProcess.HasEffects()) {
			const VkImage sceneImage = postProcess.TryGetSceneImage(currentImageIndex);
			if (!sceneInputPassReady && !commandBuffer.EndSceneColorPass(currentImageIndex, sceneImage)) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"장면 색상 패스 종료", "Vulkan 장면 색상을 후처리 입력 상태로 전환하지 못했습니다"));
			}
			if (!postProcess.Draw(commandBuffer, currentImageIndex,
				swapChain.GetImage(currentImageIndex), swapChain.GetImageView(currentImageIndex),
				GetPostProcessFrameData())) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"후처리 효과 draw", "Vulkan 후처리 chain 실행에 실패했습니다"));
			}
		} else if (!commandBuffer.EndRendering(currentImageIndex)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"장면 패스 종료", "Vulkan 장면 렌더링을 끝내지 못했습니다"));
		}
		if (!commandBuffer.EndRecord(currentImageIndex, swapChain.GetImage(currentImageIndex)))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"command buffer 종료", "Vulkan 출력 패스 기록을 끝내지 못했습니다"));
		const VkCommandBuffer nativeCommandBuffer = commandBuffer.TryGetCommandBuffer(currentImageIndex);
		if (!syncObject.Submit(device.graphicsQueue, nativeCommandBuffer, currentImageIndex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandSubmissionFailed,
				"프레임 제출", "Vulkan command buffer를 제출하지 못했습니다"));
		const VkSemaphore renderFinishedSemaphore = syncObject.GetRenderFinishedSemaphore(currentImageIndex);
		const VkSwapchainKHR swapChains[] = { swapChain.GetSwapChain() };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &currentImageIndex;
		const VkResult presentResult = vkQueuePresentKHR(device.presentQueue, &presentInfo);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
			const auto recreateResult = RecreateFromFramebuffer();
			if (!recreateResult)
				return std::unexpected(recreateResult.error());
			return FrameEndState::Skipped;
		}
		if (presentResult != VK_SUCCESS)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::PresentationFailed,
				"swap chain present", "Vulkan 프레임을 표시하지 못했습니다", presentResult, true));
		syncObject.AdvanceFrame();
		return FrameEndState::Presented;
	}

	GraphicsResult<void> VulkanViewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 시작", "Vulkan draw context가 준비되지 않았습니다"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		if (!postProcess.BeginSceneInputPass(commandContext.GetCommandBuffer(),
			currentImageIndex, swapChain.GetExtent()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"후처리 장면 입력 패스 시작", "Vulkan 장면 입력 패스를 시작하지 못했습니다"));
		drawContext.ResetDescriptorBindings();
		return {};
	}

	GraphicsResult<void> VulkanViewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 종료", "Vulkan draw context가 준비되지 않았습니다"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		postProcessSceneInputPassReady = postProcess.EndSceneInputPass(commandContext.GetCommandBuffer(),
			currentImageIndex);
		if (!postProcessSceneInputPassReady)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"후처리 장면 입력 패스 종료", "Vulkan 장면 입력 패스를 끝내지 못했습니다"));
		return {};
	}

	GraphicsResult<void> VulkanViewer::WaitIdle() {
		if (device.device == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"GPU 대기", "Vulkan device를 사용할 수 없습니다"));
		const VkResult waitResult = vkDeviceWaitIdle(device.device);
		if (waitResult != VK_SUCCESS)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"GPU 대기", "Vulkan device가 idle 상태가 되지 않았습니다", waitResult, true));
		return {};
	}

	GraphicsResult<void> VulkanViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (device.device == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 효과 구성", "Vulkan device를 사용할 수 없습니다"));
		if (!postProcess.Configure(device, swapChain, msaaDepthBuffer.format, effects))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::EffectConfigurationFailed,
				"후처리 효과 구성", "Vulkan 효과 chain을 만들지 못했습니다"));
		return {};
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstanceCore() {
		return std::make_unique<VulkanInstance>(
			device, pipeline, uploadContext, textureCache, dummyTexture, drawContext);
	}
}
