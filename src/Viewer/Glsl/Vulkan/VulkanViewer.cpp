#include "VulkanViewer.h"

#include "VulkanInstance.h"

#include <iostream>

namespace Chrivent {
	bool VulkanViewer::CreateSwapChainResources() {
		if (!msaaColorBuffer.Initialize(*device, swapChain))
			return false;
		if (!msaaDepthBuffer.Initialize(*device, swapChain))
			return false;
		if (!renderPass.Initialize(*device, swapChain, msaaDepthBuffer.format))
			return false;
		if (!pipeline->Initialize(*device, swapChain, renderPass.GetRenderPass(), shaderDir))
			return false;
		if (!frameBuffer.Initialize(*device, swapChain, renderPass.GetRenderPass(),
			msaaColorBuffer.imageView, msaaDepthBuffer.imageView))
			return false;
		return commandContext.Initialize(*device, swapChain);
	}

	void VulkanViewer::DestroySwapChainResources() {
		commandContext.Destroy();
		frameBuffer.Destroy();
		pipeline->Destroy();
		renderPass.Destroy();
		msaaColorBuffer.Destroy();
		msaaDepthBuffer.Destroy();
	}

	VulkanViewer::VulkanViewer() {
		device = std::make_shared<VulkanDevice>();
		pipeline = std::make_shared<VulkanPipeline>();
		syncObject = std::make_shared<VulkanSyncObject>();
		dummyTexture = std::make_shared<VulkanTexture>();
		bindStateCache.vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		bindStateCache.pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
	}

	void VulkanViewer::DrawIndexed(const VulkanBuffer& vertexBuffer, const VulkanBuffer& indexBuffer,
		const VkIndexType indexType, const size_t firstIndex, const size_t indexCount) {
		if (!frameReady)
			return;
		if (firstIndex > std::numeric_limits<uint32_t>::max() ||
			indexCount > std::numeric_limits<uint32_t>::max()) {
			std::cerr << "Failed to draw Vulkan model: index range is too large.\n";
			return;
		}
		auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.DrawIndexed(currentImageIndex, vertexBuffer, indexBuffer, indexType, firstIndex, indexCount);
	}

	void VulkanViewer::BindModelPipeline(const bool bothFace) {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = bothFace
			? pipeline->bothFacePipeline
			: pipeline->pipeline;
		if (bindStateCache.pipeline == targetPipeline)
			return;
		const auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanViewer::BindEdgePipeline() {
		if (!frameReady)
			return;
		if (bindStateCache.pipeline == pipeline->edgePipeline)
			return;
		const auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.BindPipeline(currentImageIndex, pipeline->edgePipeline);
		bindStateCache.pipeline = pipeline->edgePipeline;
	}

	void VulkanViewer::BindGroundShadowPipeline() {
		if (!frameReady)
			return;
		if (bindStateCache.pipeline == pipeline->groundShadowPipeline)
			return;
		const auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.BindPipeline(currentImageIndex, pipeline->groundShadowPipeline);
		bindStateCache.pipeline = pipeline->groundShadowPipeline;
	}

	void VulkanViewer::BindModelDescriptorSets(const VulkanDescriptorSet& descriptorSet, const uint32_t dynamicOffset) {
		if (!frameReady)
			return;
		if (bindStateCache.vertexDescriptorSet == descriptorSet.GetVertexDescriptorSet() &&
			bindStateCache.vertexDynamicOffset == dynamicOffset)
			return;
		const auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.BindDescriptorSets(
			currentImageIndex, pipeline->pipelineLayout, 0, &descriptorSet.GetVertexDescriptorSet(), 1, &dynamicOffset, 1);
		bindStateCache.vertexDescriptorSet = descriptorSet.GetVertexDescriptorSet();
		bindStateCache.vertexDynamicOffset = dynamicOffset;
	}

	void VulkanViewer::BindPixelDescriptorSet(const VkDescriptorSet descriptorSet, const uint32_t dynamicOffset) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		if (bindStateCache.pixelDescriptorSet == descriptorSet &&
			bindStateCache.pixelDynamicOffset == dynamicOffset)
			return;
		const auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.BindDescriptorSets(currentImageIndex, pipeline->pipelineLayout, 1, &descriptorSet, 1, &dynamicOffset, 1);
		bindStateCache.pixelDescriptorSet = descriptorSet;
		bindStateCache.pixelDynamicOffset = dynamicOffset;
	}

	void VulkanViewer::BindTextureDescriptorSet(const VkDescriptorSet descriptorSet) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		if (bindStateCache.textureDescriptorSet == descriptorSet)
			return;
		const auto& commandBuffer = commandContext.commandBuffer;
		commandBuffer.BindDescriptorSets(currentImageIndex, pipeline->pipelineLayout, 2, &descriptorSet, 1);
		bindStateCache.textureDescriptorSet = descriptorSet;
	}

	void VulkanViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool VulkanViewer::Setup() {
		InitDirs("shader_glsl");
		if (!device->Initialize(window))
			return false;
		if (!swapChain.Initialize(*device, window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		*dummyTexture = textureCache.CreateWhiteTexture(*device, commandContext.commandPool);
		if (dummyTexture->image == VK_NULL_HANDLE)
			return false;
		return syncObject->Initialize(*device, swapChain.images.size());
	}

	bool VulkanViewer::Resize() {
		if (device->device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(device->device);
		DestroySwapChainResources();
		if (!swapChain.Recreate(*device, window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		return syncObject->ResetImageTracking(swapChain.images.size());
	}

	void VulkanViewer::BeginFrame() {
		frameReady = false;
		bindStateCache.vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		bindStateCache.pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
		const size_t frameIndex = syncObject->currentFrame;
		vkWaitForFences(device->device, 1, &syncObject->inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
		const VkResult acquireResult = vkAcquireNextImageKHR(device->device, swapChain.swapChain, UINT64_MAX,
			syncObject->imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &currentImageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			Resize();
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			std::cerr << "Failed to acquire Vulkan swapchain image.\n";
			return;
		}
		if (currentImageIndex < syncObject->imagesInFlight.size() &&
			syncObject->imagesInFlight[currentImageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(device->device, 1, &syncObject->imagesInFlight[currentImageIndex], VK_TRUE, UINT64_MAX);
		}
		auto& commandBuffer = commandContext.commandBuffer;
		vkResetCommandBuffer(commandBuffer.ResolveCommandBuffer(currentImageIndex), 0);
		const auto& frameBuffers = frameBuffer.GetFrameBuffers();
		if (!commandBuffer.BeginRecord(currentImageIndex, renderPass.GetRenderPass(),
			frameBuffers[currentImageIndex], pipeline->pipeline, swapChain.extent, clearColor))
			return;
		bindStateCache.pipeline = pipeline->pipeline;
		frameReady = true;
	}

	bool VulkanViewer::EndFrame() {
		if (!frameReady)
			return true;
		if (!commandContext.commandBuffer.EndRecord(currentImageIndex)) {
			frameReady = false;
			return false;
		}
		const auto& imageAvailableSemaphores = syncObject->imageAvailableSemaphores;
		const auto& renderFinishedSemaphores = syncObject->renderFinishedSemaphores;
		const auto& inFlightFences = syncObject->inFlightFences;
		const size_t frameIndex = syncObject->currentFrame;
		const VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[frameIndex] };
		constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		const VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentImageIndex] };
		const VkCommandBuffer commandBuffers[] = {
			commandContext.commandBuffer.ResolveCommandBuffer(currentImageIndex)
		};
		const VkFence inFlightFence = inFlightFences[frameIndex];
		if (currentImageIndex < syncObject->imagesInFlight.size())
			syncObject->imagesInFlight[currentImageIndex] = inFlightFence;
		vkResetFences(device->device, 1, &inFlightFence);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = commandBuffers;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		if (vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
			std::cerr << "Failed to submit Vulkan command buffer.\n";
			return false;
		}
		const VkSwapchainKHR swapChains[] = { swapChain.swapChain };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &currentImageIndex;
		const VkResult presentResult = vkQueuePresentKHR(device->presentQueue, &presentInfo);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
			Resize();
		} else if (presentResult != VK_SUCCESS) {
			std::cerr << "Failed to present Vulkan swapchain image.\n";
			return false;
		}
		syncObject->AdvanceFrame();
		frameReady = false;
		return true;
	}

	void VulkanViewer::WaitIdle() {
		if (device->device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(device->device);
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstance() const {
		return std::make_unique<VulkanInstance>();
	}

	VulkanTexture VulkanViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
		return textureCache.Load(*device, commandContext.commandPool, texturePath, clamp);
	}
}
