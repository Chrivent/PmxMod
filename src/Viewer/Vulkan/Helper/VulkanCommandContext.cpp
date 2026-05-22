#include "VulkanCommandContext.h"

#include <iostream>

namespace Chrivent {
	VulkanCommandContext::~VulkanCommandContext() {
		Destroy();
	}

	bool VulkanCommandContext::Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = deviceInfo.queueFamilies.graphicsFamily;
		if (vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan command pool.\n";
			return false;
		}
		return commandBuffer.Initialize(deviceInfo, commandPool, swapChainInfo);
	}

	void VulkanCommandContext::Destroy() {
		commandBuffer.Destroy();
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(device, commandPool, nullptr);
			commandPool = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
