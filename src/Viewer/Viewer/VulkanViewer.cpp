#include "Viewer/Viewer/VulkanViewer.h"

#include "Viewer/Instance/VulkanInstance.h"

namespace Chrivent {
	GraphicsResult<void> VulkanViewer::CreateSwapChainResources() {
		const auto colorResult = msaaColorBuffer.Initialize(device, swapChain);
		if (!colorResult)
			return std::unexpected(colorResult.error());
		const auto depthResult = msaaDepthBuffer.Initialize(device, swapChain);
		if (!depthResult)
			return std::unexpected(depthResult.error());
		if (postProcess.HasEffects()) {
			const auto postProcessResult = postProcess.InitializeTargets(
				device, swapChain, msaaDepthBuffer.format);
			if (!postProcessResult)
				return std::unexpected(postProcessResult.error());
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
		const auto deviceResult = device.Initialize(window, capabilities);
		if (!deviceResult)
			return std::unexpected(deviceResult.error());
		const auto swapChainResult = swapChain.Initialize(device, window);
		if (!swapChainResult)
			return std::unexpected(swapChainResult.error());
		const auto resourceResult = CreateSwapChainResources();
		if (!resourceResult)
			return std::unexpected(resourceResult.error());
		const auto pipelineResult = pipeline.Initialize(device,
			swapChain.GetImageFormat(), msaaDepthBuffer.format, shaderContract);
		if (!pipelineResult)
			return std::unexpected(pipelineResult.error());
		const auto dummyResult = textureCache.CreateWhiteTexture(device);
		if (!dummyResult)
			return std::unexpected(dummyResult.error());
		dummyTexture = *dummyResult;
		const auto syncResult = syncObject.Initialize(device, swapChain.GetImageCount());
		if (!syncResult)
			return std::unexpected(syncResult.error());
		return {};
	}

	GraphicsResult<void> VulkanViewer::ResizeCore() {
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		ResetSwapChainResources();
		const auto recreateResult = swapChain.Recreate(device, window);
		if (!recreateResult)
			return std::unexpected(recreateResult.error());
		const auto resourceResult = CreateSwapChainResources();
		if (!resourceResult)
			return std::unexpected(resourceResult.error());
		if (!pipeline.IsCompatible(swapChain.GetImageFormat(),
			msaaDepthBuffer.format, device.GetMsaaSampleCount()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ContractViolation,
				"크기 변경 후 pipeline 검증", "Vulkan pipeline이 새 swap chain과 호환되지 않습니다"));
		const auto syncResult = syncObject.ResetImageTracking(swapChain.GetImageCount());
		if (!syncResult)
			return std::unexpected(syncResult.error());
		return {};
	}

	GraphicsResult<FrameBeginState> VulkanViewer::BeginFrameCore() {
		drawContext.EndFrame();
		postProcessSceneInputPassReady = false;
		const size_t frameIndex = syncObject.GetCurrentFrameIndex();
		const auto frameWaitResult = syncObject.WaitForCurrentFrame();
		if (!frameWaitResult)
			return std::unexpected(frameWaitResult.error());
		uint32_t currentImageIndex = 0;
		const auto acquireResult = swapChain.AcquireNextImage(
			syncObject.GetImageAvailableSemaphore(), currentImageIndex);
		if (!acquireResult)
			return std::unexpected(acquireResult.error());
		if (*acquireResult == VulkanSwapChainState::RecreateRequired) {
			const auto recreateResult = RecreateFromFramebuffer();
			if (!recreateResult)
				return std::unexpected(recreateResult.error());
			return FrameBeginState::Skipped;
		}
		const auto imageWaitResult = syncObject.WaitForImage(currentImageIndex);
		if (!imageWaitResult)
			return std::unexpected(imageWaitResult.error());
		auto& commandBuffer = commandContext.GetCommandBuffer();
		const auto resetResult = commandBuffer.ResetRecord(currentImageIndex);
		if (!resetResult)
			return std::unexpected(resetResult.error());
		const VkImage resolveImage = postProcess.HasEffects()
			? postProcess.TryGetSceneImage(currentImageIndex)
			: swapChain.GetImage(currentImageIndex);
		const VkImageView resolveImageView = postProcess.HasEffects()
			? postProcess.TryGetSceneImageView(currentImageIndex)
			: swapChain.GetImageView(currentImageIndex);
		const auto recordResult = commandBuffer.BeginRecord(currentImageIndex,
			msaaColorBuffer.GetImage(), msaaColorBuffer.imageView, resolveImage, resolveImageView,
			msaaDepthBuffer.GetImage(), msaaDepthBuffer.imageView,
			VulkanMsaaDepthBuffer::HasStencilComponent(msaaDepthBuffer.format),
			device.GetMsaaSampleCount(), swapChain.GetExtent(), clearColor);
		if (!recordResult)
			return std::unexpected(recordResult.error());
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
				postProcess.DiscardImageStateFrame();
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"장면 색상 패스 종료", "Vulkan 장면 색상을 후처리 입력 상태로 전환하지 못했습니다"));
			}
			if (!postProcess.Draw(commandBuffer, currentImageIndex,
				swapChain.GetImage(currentImageIndex), swapChain.GetImageView(currentImageIndex),
				GetPostProcessFrameData())) {
				postProcess.DiscardImageStateFrame();
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"후처리 효과 draw", "Vulkan 후처리 chain 실행에 실패했습니다"));
			}
		} else if (!commandBuffer.EndRendering(currentImageIndex)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"장면 패스 종료", "Vulkan 장면 렌더링을 끝내지 못했습니다"));
		}
		const auto endRecordResult = commandBuffer.EndRecord(
			currentImageIndex, swapChain.GetImage(currentImageIndex));
		if (!endRecordResult) {
			postProcess.DiscardImageStateFrame();
			return std::unexpected(endRecordResult.error());
		}
		const VkCommandBuffer nativeCommandBuffer = commandBuffer.TryGetCommandBuffer(currentImageIndex);
		const auto submitResult = syncObject.Submit(
			device.GetGraphicsQueue(), nativeCommandBuffer, currentImageIndex);
		if (!submitResult) {
			postProcess.DiscardImageStateFrame();
			return std::unexpected(submitResult.error());
		}
		postProcess.CommitImageStateFrame();
		const VkSemaphore renderFinishedSemaphore = syncObject.GetRenderFinishedSemaphore(currentImageIndex);
		const auto presentResult = swapChain.Present(
			device.GetPresentQueue(), renderFinishedSemaphore, currentImageIndex);
		if (!presentResult)
			return std::unexpected(presentResult.error());
		if (*presentResult == VulkanSwapChainState::RecreateRequired) {
			const auto recreateResult = RecreateFromFramebuffer();
			if (!recreateResult)
				return std::unexpected(recreateResult.error());
			return FrameEndState::Skipped;
		}
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
		if (!postProcessSceneInputPassReady) {
			postProcess.DiscardImageStateFrame();
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"후처리 장면 입력 패스 종료", "Vulkan 장면 입력 패스를 끝내지 못했습니다"));
		}
		return {};
	}

	GraphicsResult<void> VulkanViewer::WaitIdle() {
		return device.WaitIdle();
	}

	GraphicsResult<void> VulkanViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (device.GetDevice() == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 효과 구성", "Vulkan device를 사용할 수 없습니다"));
		return postProcess.Configure(device, swapChain, msaaDepthBuffer.format, effects);
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstanceCore() {
		return std::make_unique<VulkanInstance>(
			device, pipeline, uploadContext, textureCache, dummyTexture, drawContext);
	}
}
