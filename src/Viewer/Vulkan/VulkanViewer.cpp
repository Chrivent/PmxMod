#include "VulkanViewer.h"

#include "VulkanInstance.h"

#include <iostream>

namespace Chrivent {
	VulkanViewer::VulkanViewer() {
		info = std::make_unique<VulkanViewerInfo>();
	}

	void VulkanViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool VulkanViewer::Setup() {
		InitDirs("shader_glsl");
		if (!device.Initialize(GetInfo().window))
			return false;
		if (!swapChain.Initialize(device.GetInfo(), GetInfo().window))
			return false;
		if (!colorBuffer.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!depthBuffer.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!renderPass.Initialize(device.GetInfo(), swapChain.GetInfo(), depthBuffer.GetInfo().format))
			return false;
		if (!pipeline.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass(), GetInfo().shaderDir))
			return false;
		if (!frameBuffer.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass(), colorBuffer.GetInfo().imageView, depthBuffer.GetInfo().imageView))
			return false;
		if (!commandContext.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		dummyTexture = textureCache.CreateWhiteTexture(device.GetInfo(), commandContext.GetCommandPool());
		if (dummyTexture.image == VK_NULL_HANDLE)
			return false;
		return syncObject.Initialize(device.GetInfo());
	}

	bool VulkanViewer::Resize() {
		if (device.GetInfo().device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(device.GetInfo().device);
		commandContext.Destroy();
		frameBuffer.Destroy();
		pipeline.Destroy();
		renderPass.Destroy();
		colorBuffer.Destroy();
		depthBuffer.Destroy();
		if (!swapChain.Recreate(device.GetInfo(), GetInfo().window))
			return false;
		if (!colorBuffer.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!depthBuffer.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!renderPass.Initialize(device.GetInfo(), swapChain.GetInfo(), depthBuffer.GetInfo().format))
			return false;
		if (!pipeline.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass(), GetInfo().shaderDir))
			return false;
		if (!frameBuffer.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass(), colorBuffer.GetInfo().imageView, depthBuffer.GetInfo().imageView))
			return false;
		return commandContext.Initialize(device.GetInfo(), swapChain.GetInfo());
	}

	void VulkanViewer::BeginFrame() {
		frameReady = false;
		const auto& deviceInfo = device.GetInfo();
		const auto& syncInfo = syncObject.GetInfo();
		const size_t frameIndex = syncInfo.currentFrame;
		const VkFence inFlightFence = syncInfo.inFlightFences[frameIndex];
		vkWaitForFences(deviceInfo.device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
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
		const auto& commandBuffer = commandContext.GetCommandBuffer();
		vkResetCommandBuffer(commandBuffer.GetCommandBuffer(currentImageIndex), 0);
		const auto& frameBuffers = frameBuffer.GetFrameBuffers();
		if (!commandBuffer.BeginRecord(
			currentImageIndex,
			renderPass.GetRenderPass(),
			frameBuffers[currentImageIndex],
			pipeline.GetInfo().pipeline,
			swapChain.GetInfo().extent,
			clearColor))
			return;
		frameReady = true;
	}

	bool VulkanViewer::EndFrame() {
		if (!frameReady)
			return true;
		if (!commandContext.GetCommandBuffer().EndRecord(currentImageIndex)) {
			frameReady = false;
			return false;
		}
		const auto& deviceInfo = device.GetInfo();
		const auto& [imageAvailableSemaphores,
			renderFinishedSemaphores,
			inFlightFences,
			currentFrame] = syncObject.GetInfo();
		const size_t frameIndex = currentFrame;
		const VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[frameIndex] };
		constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		const VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[frameIndex] };
		const VkCommandBuffer commandBuffers[] = {
			commandContext.GetCommandBuffer().GetCommandBuffer(currentImageIndex)
		};
		const VkFence inFlightFence = inFlightFences[frameIndex];
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
		syncObject.AdvanceFrame();
		frameReady = false;
		return true;
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstance() const {
		return std::make_unique<VulkanInstance>();
	}

	VulkanTexture VulkanViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
		return textureCache.Load(device.GetInfo(), commandContext.GetCommandPool(), texturePath, clamp);
	}

	void VulkanViewer::DrawIndexed(
		const VulkanBufferInfo& vertexBuffer,
		const VulkanBufferInfo& indexBuffer,
		const VkIndexType indexType,
		const size_t firstIndex,
		const size_t indexCount) const {
		if (!frameReady)
			return;
		if (firstIndex > std::numeric_limits<uint32_t>::max() ||
			indexCount > std::numeric_limits<uint32_t>::max()) {
			std::cerr << "Failed to draw Vulkan model: index range is too large.\n";
			return;
		}
		commandContext.GetCommandBuffer().DrawIndexed(
			currentImageIndex,
			vertexBuffer,
			indexBuffer,
			indexType,
			firstIndex,
			indexCount);
	}

	void VulkanViewer::BindModelDescriptorSets(const VulkanDescriptorSetInfo& descriptorSetInfo) const {
		if (!frameReady)
			return;
		commandContext.GetCommandBuffer().BindDescriptorSets(
			currentImageIndex,
			pipeline.GetInfo().pipelineLayout,
			0,
			&descriptorSetInfo.vertexDescriptorSet,
			1);
	}

	void VulkanViewer::BindPixelDescriptorSet(const VkDescriptorSet descriptorSet) const {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		commandContext.GetCommandBuffer().BindDescriptorSets(
			currentImageIndex,
			pipeline.GetInfo().pipelineLayout,
			1,
			&descriptorSet,
			1);
	}

	void VulkanViewer::BindModelPipeline(const bool bothFace) const {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = bothFace
			? pipeline.GetInfo().bothFacePipeline
			: pipeline.GetInfo().pipeline;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline);
	}

	void VulkanViewer::BindEdgePipeline() const {
		if (!frameReady)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, pipeline.GetInfo().edgePipeline);
	}

	void VulkanViewer::BindGroundShadowPipeline() const {
		if (!frameReady)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, pipeline.GetInfo().groundShadowPipeline);
	}

	void VulkanViewer::BindTextureDescriptorSet(const VkDescriptorSet descriptorSet) const {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		commandContext.GetCommandBuffer().BindDescriptorSets(
			currentImageIndex,
			pipeline.GetInfo().pipelineLayout,
			2,
			&descriptorSet,
			1);
	}
}
