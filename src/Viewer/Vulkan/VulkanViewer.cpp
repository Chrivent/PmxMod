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
		InitDirs("shader_Vulkan");
		if (!device.Initialize(GetInfo().window))
			return false;
		if (!swapChain.Initialize(device.GetInfo(), GetInfo().window))
			return false;
		if (!renderPass.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!frameBuffer.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass()))
			return false;
		if (!commandContext.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		return syncObject.Initialize(device.GetInfo());
	}

	bool VulkanViewer::Resize() {
		commandContext.Destroy();
		frameBuffer.Destroy();
		renderPass.Destroy();
		if (!swapChain.Recreate(device.GetInfo(), GetInfo().window))
			return false;
		if (!renderPass.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!frameBuffer.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass()))
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
		if (!commandBuffer.Record(
			currentImageIndex,
			renderPass.GetRenderPass(),
			frameBuffers[currentImageIndex],
			swapChain.GetInfo().extent,
			clearColor))
			return;
		frameReady = true;
	}

	bool VulkanViewer::EndFrame() {
		return true;
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstance() const {
		return std::make_unique<VulkanInstance>();
	}

	VulkanTexture VulkanViewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(texturePath);
	}
}
