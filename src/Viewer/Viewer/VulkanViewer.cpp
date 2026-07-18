#include "Viewer/Viewer/VulkanViewer.h"

#include "Viewer/Instance/VulkanInstance.h"

namespace Chrivent {
	bool VulkanViewer::CreateSwapChainResources() {
		if (!msaaColorBuffer.Initialize(device, swapChain))
			return false;
		if (!msaaDepthBuffer.Initialize(device, swapChain))
			return false;
		if (postProcess.HasEffects()) {
			if (!postProcess.Initialize(device, swapChain, msaaDepthBuffer.format))
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
				"initialize device", "the Vulkan device could not be created"));
		capabilities = device.capabilities;
		if (!swapChain.Initialize(device, window))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize swap chain", "the Vulkan swap chain could not be created"));
		if (!CreateSwapChainResources())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize swap chain resources", "the Vulkan frame resources could not be created"));
		if (!pipeline.Initialize(device, swapChain.imageFormat, msaaDepthBuffer.format,
			shaderContract.builtIn,
			shaderContract.sceneInput.depth, shaderContract.sceneInput.velocity))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize rendering pipeline", "the Vulkan pipeline could not be created"));
		dummyTexture = textureCache.CreateWhiteTexture(device);
		if (dummyTexture.image == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"create dummy texture", "the fallback texture could not be created"));
		if (!syncObject.Initialize(device, swapChain.images.size()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize frame synchronization", "the Vulkan synchronization objects could not be created"));
		return {};
	}

	GraphicsResult<void> VulkanViewer::ResizeCore() {
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		ResetSwapChainResources();
		if (!swapChain.Recreate(device, window))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize swap chain", "the Vulkan swap chain could not be recreated"));
		if (!CreateSwapChainResources())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize swap chain resources", "the Vulkan frame resources could not be recreated"));
		if (!pipeline.IsCompatible(swapChain.imageFormat,
			msaaDepthBuffer.format, device.msaaSampleCount))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ContractViolation,
				"validate resized pipeline", "the Vulkan pipeline is incompatible with the new swap chain"));
		if (!syncObject.ResetImageTracking(swapChain.images.size()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"resize frame synchronization", "Vulkan image tracking could not be reset"));
		return {};
	}

	GraphicsResult<FrameBeginState> VulkanViewer::BeginFrameCore() {
		drawContext.EndFrame();
		postProcessSceneInputPassReady = false;
		const size_t frameIndex = syncObject.GetCurrentFrameIndex();
		if (!syncObject.WaitForCurrentFrame())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"wait for current frame", "the Vulkan frame fence wait failed"));
		uint32_t currentImageIndex = 0;
		const VkResult acquireResult = vkAcquireNextImageKHR(device.device, swapChain.swapChain, UINT64_MAX,
			syncObject.GetImageAvailableSemaphore(), VK_NULL_HANDLE, &currentImageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			const auto recreateResult = RecreateFromFramebuffer();
			if (!recreateResult)
				return std::unexpected(recreateResult.error());
			return FrameBeginState::Skipped;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::PresentationFailed,
				"acquire swap chain image", "Vulkan could not acquire the next image", acquireResult, true));
		if (!syncObject.WaitForImage(currentImageIndex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"wait for swap chain image", "the Vulkan image fence wait failed"));
		auto& commandBuffer = commandContext.commandBuffer;
		const VkResult resetResult = vkResetCommandBuffer(commandBuffer.TryGetCommandBuffer(currentImageIndex), 0);
		if (resetResult != VK_SUCCESS)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"reset command buffer", "the Vulkan command buffer could not be reset", resetResult, true));
		const VkImage resolveImage = postProcess.HasEffects()
			? postProcess.TryGetSceneImage(currentImageIndex)
			: swapChain.images[currentImageIndex];
		const VkImageView resolveImageView = postProcess.HasEffects()
			? postProcess.TryGetSceneImageView(currentImageIndex)
			: swapChain.imageViews[currentImageIndex];
		if (!commandBuffer.BeginRecord(currentImageIndex,
			msaaColorBuffer.GetImage(), msaaColorBuffer.imageView, resolveImage, resolveImageView,
			msaaDepthBuffer.GetImage(), msaaDepthBuffer.imageView,
			VulkanMsaaDepthBuffer::HasStencilComponent(msaaDepthBuffer.format),
			device.msaaSampleCount, pipeline.ResolveModelPipeline(false), swapChain.extent, clearColor))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"begin command buffer", "the Vulkan frame command buffer could not begin recording"));
		drawContext.BeginFrame(currentImageIndex, frameIndex);
		drawContext.SetPipelineState(pipeline.ResolveModelPipeline(false));
		return FrameBeginState::Ready;
	}

	GraphicsResult<FrameEndState> VulkanViewer::EndFrameCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end frame", "the Vulkan draw context is not ready"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		const bool sceneInputPassReady = postProcessSceneInputPassReady;
		drawContext.EndFrame();
		postProcessSceneInputPassReady = false;
		bool recordEnded;
		if (postProcess.HasEffects()) {
			recordEnded = postProcess.EndRecord(commandContext.commandBuffer, currentImageIndex,
				swapChain.images[currentImageIndex], swapChain.imageViews[currentImageIndex],
				swapChain.extent, GetPostProcessFrameData(), sceneInputPassReady);
		} else
			recordEnded = commandContext.commandBuffer.EndRecord(currentImageIndex, swapChain.images[currentImageIndex]);
		if (!recordEnded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"end command buffer", "the Vulkan output pass could not finish recording"));
		const VkCommandBuffer commandBuffer = commandContext.commandBuffer.TryGetCommandBuffer(currentImageIndex);
		if (!syncObject.Submit(device.graphicsQueue, commandBuffer, currentImageIndex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandSubmissionFailed,
				"submit frame", "the Vulkan command buffer could not be submitted"));
		const VkSemaphore renderFinishedSemaphore = syncObject.GetRenderFinishedSemaphore(currentImageIndex);
		const VkSwapchainKHR swapChains[] = { swapChain.swapChain };
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
				"present swap chain", "the Vulkan frame could not be presented", presentResult, true));
		syncObject.AdvanceFrame();
		return FrameEndState::Presented;
	}

	GraphicsResult<void> VulkanViewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"begin post-process scene input pass", "the Vulkan draw context is not ready"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		const VkPipeline geometryPipeline = pipeline.ResolveSceneInputPipeline(
			postProcess.RequiresVelocity(), false);
		if (!postProcess.BeginSceneInputPass(commandContext.commandBuffer,
			currentImageIndex, geometryPipeline, swapChain.extent))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"begin post-process scene input pass", "the Vulkan scene input pass could not begin"));
		drawContext.SetPipelineState(geometryPipeline);
		drawContext.ResetDescriptorBindings();
		return {};
	}

	GraphicsResult<void> VulkanViewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end post-process scene input pass", "the Vulkan draw context is not ready"));
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		postProcessSceneInputPassReady = postProcess.EndSceneInputPass(commandContext.commandBuffer,
			currentImageIndex);
		if (!postProcessSceneInputPassReady)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"end post-process scene input pass", "the Vulkan scene input pass could not end"));
		return {};
	}

	GraphicsResult<void> VulkanViewer::WaitIdle() {
		if (device.device == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"wait for GPU", "the Vulkan device is unavailable"));
		const VkResult waitResult = vkDeviceWaitIdle(device.device);
		if (waitResult != VK_SUCCESS)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"wait for GPU", "the Vulkan device did not become idle", waitResult, true));
		return {};
	}

	GraphicsResult<void> VulkanViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (device.device == VK_NULL_HANDLE)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"configure post-process effects", "the Vulkan device is unavailable"));
		if (!postProcess.Configure(device, swapChain, msaaDepthBuffer.format, effects))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::EffectConfigurationFailed,
				"configure post-process effects", "the Vulkan effect chain could not be created"));
		return {};
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstanceCore() {
		return std::make_unique<VulkanInstance>(
			*this, device, pipeline, uploadContext, textureCache, dummyTexture, drawContext);
	}
}
