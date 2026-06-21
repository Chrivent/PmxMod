#include "VulkanViewer.h"

#include "VulkanInstance.h"

#include <iostream>

namespace Chrivent {
	bool VulkanViewer::CreateSwapChainResources() {
		if (!msaaColorBuffer.Initialize(device->GetInfo(), swapChain.GetInfo()))
			return false;
		if (!msaaDepthBuffer.Initialize(device->GetInfo(), swapChain.GetInfo()))
			return false;
		if (!renderPass.Initialize(device->GetInfo(), swapChain.GetInfo(), msaaDepthBuffer.GetInfo().format))
			return false;
		if (!pipeline->Initialize(device->GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass(), GetInfo().shaderDir))
			return false;
		if (!frameBuffer.Initialize(
			device->GetInfo(),
			swapChain.GetInfo(),
			renderPass.GetRenderPass(),
			msaaColorBuffer.GetInfo().imageView,
			msaaDepthBuffer.GetInfo().imageView))
			return false;
		return commandContext.Initialize(device->GetInfo(), swapChain.GetInfo());
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
		info = std::make_unique<VulkanViewerInfo>();
		auto& vulkanInfo = GetVulkanInfo();
		vulkanInfo.deviceInfo = std::shared_ptr<const VulkanDeviceInfo>(device, &device->GetInfo());
		vulkanInfo.pipelineInfo = std::shared_ptr<const VulkanPipelineInfo>(pipeline, &pipeline->GetInfo());
		vulkanInfo.dummyTexture = dummyTexture;
		vulkanInfo.syncInfo = std::shared_ptr<const VulkanSyncObjectInfo>(syncObject, &syncObject->GetInfo());
		bindStateCache.vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		bindStateCache.pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
	}

	void VulkanViewer::DrawIndexed(
		const VulkanBufferInfo& vertexBuffer,
		const VulkanBufferInfo& indexBuffer,
		const VkIndexType indexType,
		const size_t firstIndex,
		const size_t indexCount) {
		if (!frameReady)
			return;
		if (firstIndex > std::numeric_limits<uint32_t>::max() ||
			indexCount > std::numeric_limits<uint32_t>::max()) {
			std::cerr << "Failed to draw Vulkan model: index range is too large.\n";
			return;
		}
		auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.DrawIndexed(currentImageIndex, vertexBuffer, indexBuffer, indexType, firstIndex, indexCount);
	}

	void VulkanViewer::BindModelPipeline(const bool bothFace) {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = bothFace
			? pipeline->GetInfo().bothFacePipeline
			: pipeline->GetInfo().pipeline;
		if (bindStateCache.pipeline == targetPipeline)
			return;
		const auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanViewer::BindEdgePipeline() {
		if (!frameReady)
			return;
		if (bindStateCache.pipeline == pipeline->GetInfo().edgePipeline)
			return;
		const auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.BindPipeline(currentImageIndex, pipeline->GetInfo().edgePipeline);
		bindStateCache.pipeline = pipeline->GetInfo().edgePipeline;
	}

	void VulkanViewer::BindGroundShadowPipeline() {
		if (!frameReady)
			return;
		if (bindStateCache.pipeline == pipeline->GetInfo().groundShadowPipeline)
			return;
		const auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.BindPipeline(currentImageIndex, pipeline->GetInfo().groundShadowPipeline);
		bindStateCache.pipeline = pipeline->GetInfo().groundShadowPipeline;
	}

	void VulkanViewer::BindModelDescriptorSets(const VulkanDescriptorSet& descriptorSet, const uint32_t dynamicOffset) {
		if (!frameReady)
			return;
		if (bindStateCache.vertexDescriptorSet == descriptorSet.GetVertexDescriptorSet() &&
			bindStateCache.vertexDynamicOffset == dynamicOffset)
			return;
		const auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.BindDescriptorSets(
			currentImageIndex,
			pipeline->GetInfo().pipelineLayout,
			0,
			&descriptorSet.GetVertexDescriptorSet(),
			1,
			&dynamicOffset,
			1);
		bindStateCache.vertexDescriptorSet = descriptorSet.GetVertexDescriptorSet();
		bindStateCache.vertexDynamicOffset = dynamicOffset;
	}

	void VulkanViewer::BindPixelDescriptorSet(const VkDescriptorSet descriptorSet, const uint32_t dynamicOffset) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		if (bindStateCache.pixelDescriptorSet == descriptorSet &&
			bindStateCache.pixelDynamicOffset == dynamicOffset)
			return;
		const auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.BindDescriptorSets(
			currentImageIndex,
			pipeline->GetInfo().pipelineLayout,
			1,
			&descriptorSet,
			1,
			&dynamicOffset,
			1);
		bindStateCache.pixelDescriptorSet = descriptorSet;
		bindStateCache.pixelDynamicOffset = dynamicOffset;
	}

	void VulkanViewer::BindTextureDescriptorSet(const VkDescriptorSet descriptorSet) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		if (bindStateCache.textureDescriptorSet == descriptorSet)
			return;
		const auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		commandBuffer.BindDescriptorSets(currentImageIndex, pipeline->GetInfo().pipelineLayout, 2, &descriptorSet, 1);
		bindStateCache.textureDescriptorSet = descriptorSet;
	}

	void VulkanViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool VulkanViewer::Setup() {
		InitDirs("shader_glsl");
		if (!device->Initialize(GetInfo().window))
			return false;
		if (!swapChain.Initialize(device->GetInfo(), GetInfo().window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		*dummyTexture = textureCache.CreateWhiteTexture(device->GetInfo(), commandContext.GetInfo().commandPool);
		if (dummyTexture->image == VK_NULL_HANDLE)
			return false;
		return syncObject->Initialize(device->GetInfo(), swapChain.GetInfo().images.size());
	}

	bool VulkanViewer::Resize() {
		if (device->GetInfo().device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(device->GetInfo().device);
		DestroySwapChainResources();
		if (!swapChain.Recreate(device->GetInfo(), GetInfo().window))
			return false;
		if (!CreateSwapChainResources())
			return false;
		syncObject->ResetImageTracking(swapChain.GetInfo().images.size());
		return true;
	}

	void VulkanViewer::BeginFrame() {
		frameReady = false;
		bindStateCache.vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		bindStateCache.pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
		const auto& deviceInfo = device->GetInfo();
		const auto& syncInfo = syncObject->GetInfo();
		const size_t frameIndex = syncInfo.currentFrame;
		vkWaitForFences(deviceInfo.device, 1, &syncInfo.inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
		const VkResult acquireResult = vkAcquireNextImageKHR(
			deviceInfo.device,
			swapChain.GetInfo().swapChain,
			UINT64_MAX,
			syncInfo.imageAvailableSemaphores[frameIndex],
			VK_NULL_HANDLE,
			&currentImageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			Resize();
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			std::cerr << "Failed to acquire Vulkan swapchain image.\n";
			return;
		}
		if (currentImageIndex < syncInfo.imagesInFlight.size() &&
			syncInfo.imagesInFlight[currentImageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(deviceInfo.device, 1, &syncInfo.imagesInFlight[currentImageIndex], VK_TRUE, UINT64_MAX);
		}
		auto& commandBuffer = commandContext.GetInfo().commandBuffer;
		vkResetCommandBuffer(commandBuffer.ResolveCommandBuffer(currentImageIndex), 0);
		const auto& frameBuffers = frameBuffer.GetFrameBuffers();
		if (!commandBuffer.BeginRecord(
			currentImageIndex,
			renderPass.GetRenderPass(),
			frameBuffers[currentImageIndex],
			pipeline->GetInfo().pipeline,
			swapChain.GetInfo().extent,
			clearColor))
			return;
		bindStateCache.pipeline = pipeline->GetInfo().pipeline;
		frameReady = true;
	}

	bool VulkanViewer::EndFrame() {
		if (!frameReady)
			return true;
		if (!commandContext.GetInfo().commandBuffer.EndRecord(currentImageIndex)) {
			frameReady = false;
			return false;
		}
		const auto& deviceInfo = device->GetInfo();
		const auto& syncInfo = syncObject->GetInfo();
		const auto& imageAvailableSemaphores = syncInfo.imageAvailableSemaphores;
		const auto& renderFinishedSemaphores = syncInfo.renderFinishedSemaphores;
		const auto& inFlightFences = syncInfo.inFlightFences;
		const size_t frameIndex = syncInfo.currentFrame;
		const VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[frameIndex] };
		constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		const VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[frameIndex] };
		const VkCommandBuffer commandBuffers[] = {
			commandContext.GetInfo().commandBuffer.ResolveCommandBuffer(currentImageIndex)
		};
		const VkFence inFlightFence = inFlightFences[frameIndex];
		if (currentImageIndex < syncObject->GetInfo().imagesInFlight.size())
			syncObject->GetInfo().imagesInFlight[currentImageIndex] = inFlightFence;
		vkResetFences(deviceInfo.device, 1, &inFlightFence);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = commandBuffers;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		if (vkQueueSubmit(deviceInfo.graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
			std::cerr << "Failed to submit Vulkan command buffer.\n";
			return false;
		}
		const VkSwapchainKHR swapChains[] = { swapChain.GetInfo().swapChain };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &currentImageIndex;
		const VkResult presentResult = vkQueuePresentKHR(deviceInfo.presentQueue, &presentInfo);
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
		if (device->GetInfo().device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(device->GetInfo().device);
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstance() const {
		return std::make_unique<VulkanInstance>();
	}

	VulkanTexture VulkanViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
		return textureCache.Load(device->GetInfo(), commandContext.GetInfo().commandPool, texturePath, clamp);
	}
}
