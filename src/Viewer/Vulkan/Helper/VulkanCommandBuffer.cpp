#include "VulkanCommandBuffer.h"

#include <iostream>

namespace Chrivent {
	VulkanCommandBuffer::~VulkanCommandBuffer() {
		Destroy();
	}

	bool VulkanCommandBuffer::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VkCommandPool sourceCommandPool,
		const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		commandPool = sourceCommandPool;
		commandBuffers.resize(swapChainInfo.imageViews.size());
		if (commandBuffers.empty()) {
			std::cerr << "Failed to allocate Vulkan command buffers: swapchain image view is empty.\n";
			return false;
		}
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = commandBuffers.size();
		if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan command buffers.\n";
			return false;
		}
		return true;
	}

	void VulkanCommandBuffer::Destroy() {
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE && !commandBuffers.empty())
			vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
		commandBuffers.clear();
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
	}
}
