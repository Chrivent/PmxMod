#include "Viewer/Viewer/VulkanViewer.h"

#include "Viewer/Instance/VulkanInstance.h"

#include <utility>

namespace Chrivent {
	GraphicsError::Result<void> VulkanViewer::CreateSwapChainResources() {
		const auto colorResult = msaaColorBuffer.Initialize(device, swapChain);
		if (!colorResult)
			return std::unexpected(colorResult.error());
		const auto depthResult = msaaDepthBuffer.Initialize(device, swapChain);
		if (!depthResult)
			return std::unexpected(depthResult.error());
		if (postProcess.HasEffects()) {
			const auto postProcessResult = postProcess.InitializeTargets(
				device, swapChain, msaaDepthBuffer.GetFormat());
			if (!postProcessResult)
				return std::unexpected(postProcessResult.error());
		}
		return commandContext.Initialize(device, swapChain);
	}

	void VulkanViewer::ResetSwapChainResources() {
		commandContext.Reset();
		postProcess.ResetTargets();
		msaaColorBuffer.Reset();
		msaaDepthBuffer.Reset();
	}

	GraphicsError::Result<void> VulkanViewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
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
			swapChain.GetImageFormat(), msaaDepthBuffer.GetFormat(), shaderContract);
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

	GraphicsError::Result<void> VulkanViewer::ResizeCore() {
		const auto waitResult = WaitIdleCore();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		ResetSwapChainResources();
		const auto recreateResult = swapChain.Recreate(device, window);
		if (!recreateResult)
			return std::unexpected(recreateResult.error());
		const auto resourceResult = CreateSwapChainResources();
		if (!resourceResult)
			return std::unexpected(resourceResult.error());
		const auto pipelineResult = pipeline.RecreateIfIncompatible(
			device, swapChain.GetImageFormat(), msaaDepthBuffer.GetFormat());
		if (!pipelineResult)
			return std::unexpected(pipelineResult.error());
		const auto syncResult = syncObject.ResetImageTracking(swapChain.GetImageCount());
		if (!syncResult)
			return std::unexpected(syncResult.error());
		return {};
	}

	GraphicsError::Result<FrameBeginState> VulkanViewer::BeginFrameCore() {
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
			msaaColorBuffer.GetImage(), msaaColorBuffer.GetImageView(), resolveImage, resolveImageView,
			msaaDepthBuffer.GetImage(), msaaDepthBuffer.GetImageView(),
			VulkanMsaaDepthBuffer::HasStencilComponent(msaaDepthBuffer.GetFormat()),
			device.GetMsaaSampleCount(), swapChain.GetExtent(), clearColor);
		if (!recordResult)
			return std::unexpected(recordResult.error());
		drawContext.BeginFrame(currentImageIndex, frameIndex);
		return FrameBeginState::Ready;
	}

	GraphicsError::Result<FrameEndState> VulkanViewer::EndFrameCore() {
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
			const auto drawResult = postProcess.Draw(commandBuffer, currentImageIndex,
				swapChain.GetImage(currentImageIndex), swapChain.GetImageView(currentImageIndex),
				GetPostProcessFrameData());
			if (!drawResult)
				return std::unexpected(drawResult.error());
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

	GraphicsError::Result<void> VulkanViewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 시작", "Vulkan draw context가 준비되지 않았습니다"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		const auto beginResult = postProcess.BeginSceneInputPass(
			commandContext.GetCommandBuffer(), currentImageIndex, swapChain.GetExtent());
		if (!beginResult)
			return std::unexpected(beginResult.error());
		drawContext.ResetDescriptorBindings();
		return {};
	}

	GraphicsError::Result<void> VulkanViewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 종료", "Vulkan draw context가 준비되지 않았습니다"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		const auto endResult = postProcess.EndSceneInputPass(
			commandContext.GetCommandBuffer(), currentImageIndex);
		if (!endResult)
			return std::unexpected(endResult.error());
		postProcessSceneInputPassReady = true;
		return {};
	}

	GraphicsError::Result<void> VulkanViewer::WaitIdleCore() {
		return device.WaitIdle();
	}

	GraphicsError::Result<void> VulkanViewer::LoadPostProcessEffectsCore(
		PostProcess::PreparedEffects preparedEffects) {
		if (device.GetDevice() == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 효과 구성", "Vulkan device를 사용할 수 없습니다"));
		return postProcess.Configure(
			device, swapChain, msaaDepthBuffer.GetFormat(), std::move(preparedEffects));
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstanceCore() {
		return std::make_unique<VulkanInstance>(
			device, pipeline, uploadContext, textureCache, dummyTexture, drawContext);
	}
}
