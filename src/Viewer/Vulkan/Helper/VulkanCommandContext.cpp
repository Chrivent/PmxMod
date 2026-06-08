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
		if (vkCreateCommandPool(device, &createInfo, nullptr, &info.commandPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan command pool.\n";
			return false;
		}
		return info.commandBuffer.Initialize(deviceInfo, info.commandPool, swapChainInfo);
	}

	void VulkanCommandContext::Destroy() {
		info.commandBuffer.Destroy();
		if (device != VK_NULL_HANDLE && info.commandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(device, info.commandPool, nullptr);
			info.commandPool = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
