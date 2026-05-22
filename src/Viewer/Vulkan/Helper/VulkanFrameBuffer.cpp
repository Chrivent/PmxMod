#include "VulkanFrameBuffer.h"

#include <array>
#include <iostream>

namespace Chrivent {
	VulkanFrameBuffer::~VulkanFrameBuffer() {
		Destroy();
	}

	bool VulkanFrameBuffer::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VulkanSwapChainInfo& swapChainInfo,
		const VkRenderPass renderPass) {
		device = deviceInfo.device;
		frameBuffers.resize(swapChainInfo.imageViews.size());
		for (size_t i = 0; i < swapChainInfo.imageViews.size(); i++) {
			const std::array attachments = { swapChainInfo.imageViews[i] };
			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass;
			createInfo.attachmentCount = attachments.size();
			createInfo.pAttachments = attachments.data();
			createInfo.width = swapChainInfo.extent.width;
			createInfo.height = swapChainInfo.extent.height;
			createInfo.layers = 1;
			if (vkCreateFramebuffer(deviceInfo.device, &createInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan framebuffer.\n";
				return false;
			}
		}
		return true;
	}

	void VulkanFrameBuffer::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		for (const VkFramebuffer frameBuffer : frameBuffers)
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
		frameBuffers.clear();
		device = VK_NULL_HANDLE;
	}
}
