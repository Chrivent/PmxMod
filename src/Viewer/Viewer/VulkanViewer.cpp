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
		const size_t frameIndex = syncObject.currentFrame;
		if (vkWaitForFences(device.device, 1, &syncObject.inFlightFences[frameIndex], VK_TRUE, UINT64_MAX)
			!= VK_SUCCESS)
			return FrameBeginResult::Failed;
		const VkResult acquireResult = vkAcquireNextImageKHR(device.device, swapChain.swapChain, UINT64_MAX,
			syncObject.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &currentImageIndex);
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
		if (currentImageIndex < syncObject.imagesInFlight.size() &&
			syncObject.imagesInFlight[currentImageIndex] != VK_NULL_HANDLE) {
			if (vkWaitForFences(device.device, 1, &syncObject.imagesInFlight[currentImageIndex],
				VK_TRUE, UINT64_MAX) != VK_SUCCESS)
				return FrameBeginResult::Failed;
		}
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
				swapChain.extent, postProcessFrameData, sceneInputPassReady);
		} else
			recordEnded = commandContext.commandBuffer.EndRecord(currentImageIndex, swapChain.images[currentImageIndex]);
		if (!recordEnded)
			return FrameEndResult::Failed;
		const auto& imageAvailableSemaphores = syncObject.imageAvailableSemaphores;
		const auto& renderFinishedSemaphores = syncObject.renderFinishedSemaphores;
		const auto& inFlightFences = syncObject.inFlightFences;
		const size_t frameIndex = syncObject.currentFrame;
		const VkSemaphoreSubmitInfo waitSemaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = imageAvailableSemaphores[frameIndex],
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		const VkSemaphoreSubmitInfo signalSemaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = renderFinishedSemaphores[currentImageIndex],
			.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
		};
		const VkCommandBufferSubmitInfo commandBufferInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = commandContext.commandBuffer.ResolveCommandBuffer(currentImageIndex)
		};
		const VkFence inFlightFence = inFlightFences[frameIndex];
		if (vkResetFences(device.device, 1, &inFlightFence) != VK_SUCCESS) {
			postProcess.DiscardHistoryFrame();
			std::cerr << "Failed to reset Vulkan in-flight fence.\n";
			return FrameEndResult::Failed;
		}
		const VkSubmitInfo2 submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &waitSemaphoreInfo,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &commandBufferInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = &signalSemaphoreInfo
		};
		if (vkQueueSubmit2(device.graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
			postProcess.DiscardHistoryFrame();
			std::cerr << "Failed to submit Vulkan command buffer.\n";
			return FrameEndResult::Failed;
		}
		if (currentImageIndex < syncObject.imagesInFlight.size())
			syncObject.imagesInFlight[currentImageIndex] = inFlightFence;
		postProcess.CommitHistoryFrame();
		const VkSwapchainKHR swapChains[] = { swapChain.swapChain };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentImageIndex];
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
			std::cerr << "Failed to present Vulkan swapchain image.\n";
			return FrameEndResult::Failed;
		}
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

	bool VulkanViewer::LoadPostProcessEffectsCore(const std::vector<const EffectDefinition*>& effects) {
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
