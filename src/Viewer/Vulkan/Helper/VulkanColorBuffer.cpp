#include "VulkanColorBuffer.h"

#include <iostream>

namespace Chrivent {
	bool VulkanColorBuffer::FindMemoryType(
		const VulkanDeviceInfo& deviceInfo,
		const uint32_t typeFilter,
		const VkMemoryPropertyFlags properties,
		uint32_t& memoryType) {
		VkPhysicalDeviceMemoryProperties memoryProperties{};
		vkGetPhysicalDeviceMemoryProperties(deviceInfo.physicalDevice, &memoryProperties);
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
			if ((typeFilter & 1 << i) != 0 &&
				(memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				memoryType = i;
				return true;
			}
		}
		return false;
	}

	bool VulkanColorBuffer::CreateImage(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = swapChainInfo.extent.width;
		imageInfo.extent.height = swapChainInfo.extent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = info.format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.samples = deviceInfo.sampleCount;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(deviceInfo.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan MSAA color image.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(deviceInfo.device, image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!FindMemoryType(deviceInfo, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			std::cerr << "Failed to find Vulkan MSAA color image memory type.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(deviceInfo.device, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan MSAA color image memory.\n";
			return false;
		}
		if (vkBindImageMemory(deviceInfo.device, image, imageMemory, 0) != VK_SUCCESS) {
			std::cerr << "Failed to bind Vulkan MSAA color image memory.\n";
			return false;
		}
		return true;
	}

	bool VulkanColorBuffer::CreateImageView() {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = info.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device, &viewInfo, nullptr, &info.imageView) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan MSAA color image view.\n";
			return false;
		}
		return true;
	}

	VulkanColorBuffer::~VulkanColorBuffer() {
		Destroy();
	}

	bool VulkanColorBuffer::Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		info.format = swapChainInfo.imageFormat;
		if (!CreateImage(deviceInfo, swapChainInfo))
			return false;
		return CreateImageView();
	}

	void VulkanColorBuffer::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (info.imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device, info.imageView, nullptr);
			info.imageView = VK_NULL_HANDLE;
		}
		if (image != VK_NULL_HANDLE) {
			vkDestroyImage(device, image, nullptr);
			image = VK_NULL_HANDLE;
		}
		if (imageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(device, imageMemory, nullptr);
			imageMemory = VK_NULL_HANDLE;
		}
		info.format = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}
}
