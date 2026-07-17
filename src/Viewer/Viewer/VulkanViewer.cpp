#include "Viewer/Viewer/VulkanViewer.h"

#include "Viewer/Instance/VulkanInstance.h"
#include <iostream>

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
		if (!pipeline.Initialize(device, swapChain, msaaDepthBuffer.format, builtInShaderPasses,
			sceneInputShaderPasses.depth, sceneInputShaderPasses.velocity))
			return false;
		return commandContext.Initialize(device, swapChain);
	}

	void VulkanViewer::ResetSwapChainResources() {
		commandContext.Reset();
		pipeline.Reset();
		postProcess.ResetResources();
		msaaColorBuffer.Reset();
		msaaDepthBuffer.Reset();
	}

	bool VulkanViewer::SetupCore() {
		BindPostProcess(postProcess);
		if (!device.Initialize(window))
			return false;
		capabilities = device.capabilities;
		if (!swapChain.Initialize(device, window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		dummyTexture = textureCache.CreateWhiteTexture(device);
		if (dummyTexture.image == VK_NULL_HANDLE)
			return false;
		return syncObject.Initialize(device, swapChain.images.size());
	}

	bool VulkanViewer::ResizeCore() {
		if (!WaitIdle())
			return false;
		ResetSwapChainResources();
		if (!swapChain.Recreate(device, window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		return syncObject.ResetImageTracking(swapChain.images.size());
	}

	FrameBeginResult VulkanViewer::BeginFrameCore() {
		drawContext.EndFrame();
		postProcessSceneInputPassReady = false;
		const size_t frameIndex = syncObject.GetCurrentFrameIndex();
		if (!syncObject.WaitForCurrentFrame())
			return FrameBeginResult::Failed;
		uint32_t currentImageIndex = 0;
		const VkResult acquireResult = vkAcquireNextImageKHR(device.device, swapChain.swapChain, UINT64_MAX,
			syncObject.GetImageAvailableSemaphore(), VK_NULL_HANDLE, &currentImageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			if (!ResizeCore())
				return FrameBeginResult::Failed;
			ResetPostProcessHistory();
			return FrameBeginResult::Skipped;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			std::cerr << "Failed to acquire Vulkan swapchain image.\n";
			return FrameBeginResult::Failed;
		}
		if (!syncObject.WaitForImage(currentImageIndex))
			return FrameBeginResult::Failed;
		auto& commandBuffer = commandContext.commandBuffer;
		if (vkResetCommandBuffer(commandBuffer.TryGetCommandBuffer(currentImageIndex), 0) != VK_SUCCESS)
			return FrameBeginResult::Failed;
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
			return FrameBeginResult::Failed;
		drawContext.BeginFrame(currentImageIndex, frameIndex);
		drawContext.SetPipelineState(pipeline.ResolveModelPipeline(false));
		return FrameBeginResult::Ready;
	}

	FrameEndResult VulkanViewer::EndFrameCore() {
		if (!drawContext.IsFrameReady())
			return FrameEndResult::Failed;
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
			return FrameEndResult::Failed;
		const VkCommandBuffer commandBuffer = commandContext.commandBuffer.TryGetCommandBuffer(currentImageIndex);
		if (!syncObject.Submit(device.graphicsQueue, commandBuffer, currentImageIndex)) {
			std::cerr << "Failed to submit Vulkan command buffer.\n";
			return FrameEndResult::Failed;
		}
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
			if (!ResizeCore())
				return FrameEndResult::Failed;
			ResetPostProcessHistory();
			return FrameEndResult::Skipped;
		}
		if (presentResult != VK_SUCCESS) {
			std::cerr << "Failed to present Vulkan swapchain image.\n";
			return FrameEndResult::Failed;
		}
		syncObject.AdvanceFrame();
		return FrameEndResult::Presented;
	}

	bool VulkanViewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return false;
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		const VkPipeline geometryPipeline = pipeline.ResolveSceneInputPipeline(
			postProcess.RequiresVelocity(), false);
		if (!postProcess.BeginSceneInputPass(commandContext.commandBuffer,
			currentImageIndex, geometryPipeline, swapChain.extent))
			return false;
		drawContext.SetPipelineState(geometryPipeline);
		drawContext.ResetDescriptorBindings();
		return true;
	}

	bool VulkanViewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return false;
		const uint32_t currentImageIndex = drawContext.GetCurrentImageIndex();
		postProcessSceneInputPassReady = postProcess.EndSceneInputPass(commandContext.commandBuffer,
			currentImageIndex);
		return postProcessSceneInputPassReady;
	}

	bool VulkanViewer::WaitIdle() {
		return device.device != VK_NULL_HANDLE && vkDeviceWaitIdle(device.device) == VK_SUCCESS;
	}

	bool VulkanViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return device.device != VK_NULL_HANDLE
			&& postProcess.Configure(device, swapChain, msaaDepthBuffer.format, effects);
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstanceCore() {
		return std::make_unique<VulkanInstance>(
			*this, device, pipeline, textureCache, dummyTexture, drawContext);
	}
}
