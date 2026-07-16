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

	void VulkanViewer::ConfigureWindowHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool VulkanViewer::Setup() {
		if (!InitializeShaderResources())
			return false;
		if (!device.Initialize(window))
			return false;
		capabilities = device.capabilities;
		if (!swapChain.Initialize(device, window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		dummyTexture = textureCache.CreateWhiteTexture(device, commandContext.commandPool);
		if (dummyTexture.image == VK_NULL_HANDLE)
			return false;
		return syncObject.Initialize(device, swapChain.images.size());
	}

	bool VulkanViewer::Resize() {
		if (!WaitIdle())
			return false;
		ResetSwapChainResources();
		if (!swapChain.Recreate(device, window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		return syncObject.ResetImageTracking(swapChain.images.size());
	}

	FrameBeginResult VulkanViewer::BeginFrame() {
		frameReady = false;
		postProcessSceneInputPassReady = false;
		drawContext.ResetFrameState();
		frameIndex = syncObject.GetCurrentFrameIndex();
		if (!syncObject.WaitForCurrentFrame())
			return FrameBeginResult::Failed;
		const VkResult acquireResult = vkAcquireNextImageKHR(device.device, swapChain.swapChain, UINT64_MAX,
			syncObject.GetImageAvailableSemaphore(), VK_NULL_HANDLE, &currentImageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			if (!Resize())
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
		if (vkResetCommandBuffer(commandBuffer.ResolveCommandBuffer(currentImageIndex), 0) != VK_SUCCESS)
			return FrameBeginResult::Failed;
		const VkImage resolveImage = postProcess.HasEffects()
			? postProcess.ResolveSceneImage(currentImageIndex)
			: swapChain.images[currentImageIndex];
		const VkImageView resolveImageView = postProcess.HasEffects()
			? postProcess.ResolveSceneImageView(currentImageIndex)
			: swapChain.imageViews[currentImageIndex];
		if (!commandBuffer.BeginRecord(currentImageIndex,
			msaaColorBuffer.GetImage(), msaaColorBuffer.imageView, resolveImage, resolveImageView,
			msaaDepthBuffer.GetImage(), msaaDepthBuffer.imageView,
			VulkanMsaaDepthBuffer::HasStencilComponent(msaaDepthBuffer.format),
			device.msaaSampleCount, pipeline.ResolveModelPipeline(false), swapChain.extent, clearColor))
			return FrameBeginResult::Failed;
		drawContext.SetPipelineState(pipeline.ResolveModelPipeline(false));
		frameReady = true;
		return FrameBeginResult::Ready;
	}

	FrameEndResult VulkanViewer::EndFrame() {
		if (!frameReady)
			return FrameEndResult::Failed;
		const bool sceneInputPassReady = postProcessSceneInputPassReady;
		frameReady = false;
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
		const VkCommandBuffer commandBuffer = commandContext.commandBuffer.ResolveCommandBuffer(currentImageIndex);
		if (!syncObject.Submit(device.graphicsQueue, commandBuffer, currentImageIndex)) {
			postProcess.DiscardHistoryFrame();
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
			if (!Resize())
				return FrameEndResult::Failed;
			ResetPostProcessHistory();
			return FrameEndResult::Skipped;
		}
		if (presentResult != VK_SUCCESS) {
			postProcess.DiscardHistoryFrame();
			std::cerr << "Failed to present Vulkan swapchain image.\n";
			return FrameEndResult::Failed;
		}
		postProcess.CommitHistoryFrame();
		syncObject.AdvanceFrame();
		return FrameEndResult::Presented;
	}

	bool VulkanViewer::BeginPostProcessSceneInputPassCore() {
		if (!frameReady)
			return false;
		const VkPipeline geometryPipeline = pipeline.ResolveSceneInputPipeline(
			postProcess.RequiresVelocity(), false);
		if (!postProcess.BeginSceneInputPass(commandContext.commandBuffer,
			currentImageIndex, geometryPipeline, swapChain.extent))
			return false;
		drawContext.SetPipelineState(geometryPipeline);
		drawContext.ResetDescriptorBindings();
		return true;
	}

	bool VulkanViewer::EndPostProcessSceneInputPass() {
		if (!frameReady)
			return false;
		postProcessSceneInputPassReady = postProcess.EndSceneInputPass(commandContext.commandBuffer,
			currentImageIndex);
		return postProcessSceneInputPassReady;
	}

	bool VulkanViewer::WaitIdle() {
		return device.device != VK_NULL_HANDLE && vkDeviceWaitIdle(device.device) == VK_SUCCESS;
	}

	bool VulkanViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return device.device != VK_NULL_HANDLE
			&& postProcess.Load(device, swapChain, msaaDepthBuffer.format, effects);
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstanceCore() {
		return std::make_unique<VulkanInstance>(*this);
	}

	VulkanTexture VulkanViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
		return textureCache.Load(device, commandContext.commandPool, texturePath, clamp);
	}
}
