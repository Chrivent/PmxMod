#include "Viewer/Vulkan/Helper/VulkanFrameBuffer.h"

#include <iostream>

namespace Chrivent {
	VulkanFrameBuffer::~VulkanFrameBuffer() {
		Destroy();
	}

	bool VulkanFrameBuffer::Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkRenderPass renderPass, const VkImageView colorImageView, const VkImageView depthImageView) {
		return Initialize(sourceDevice, sourceSwapChain, renderPass, colorImageView, depthImageView,
			sourceSwapChain.imageViews);
	}

	bool VulkanFrameBuffer::Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkRenderPass renderPass, const VkImageView colorImageView, const VkImageView depthImageView,
		const std::vector<VkImageView>& resolveImageViews) {
		if (resolveImageViews.size() != sourceSwapChain.imageViews.size())
			return false;
		device = sourceDevice.device;
		frameBuffers.resize(sourceSwapChain.imageViews.size());
		for (size_t i = 0; i < sourceSwapChain.imageViews.size(); i++) {
			const VkImageView attachments[] = {
				colorImageView,
				depthImageView,
				resolveImageViews[i],
			};
			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass;
			createInfo.attachmentCount = 3;
			createInfo.pAttachments = attachments;
			createInfo.width = sourceSwapChain.extent.width;
			createInfo.height = sourceSwapChain.extent.height;
			createInfo.layers = 1;
			if (vkCreateFramebuffer(sourceDevice.device, &createInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
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
