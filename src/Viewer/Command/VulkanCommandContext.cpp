#include "Viewer/Command/VulkanCommandContext.h"

#include <iostream>

namespace Chrivent {
	VulkanCommandContext::~VulkanCommandContext() {
		Reset();
	}

	bool VulkanCommandContext::Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		device = sourceDevice.device;
		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = sourceDevice.queueFamilies.graphicsFamily;
		if (vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan command pool.\n";
			return false;
		}
		return commandBuffer.Initialize(sourceDevice, commandPool, sourceSwapChain);
	}

	void VulkanCommandContext::Reset() {
		commandBuffer.Reset();
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(device, commandPool, nullptr);
			commandPool = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
