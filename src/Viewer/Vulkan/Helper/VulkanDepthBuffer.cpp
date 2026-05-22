#include "VulkanDepthBuffer.h"

#include <iostream>

namespace Chrivent {
	VulkanDepthBuffer::~VulkanDepthBuffer() {
		Destroy();
	}

	bool VulkanDepthBuffer::Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		info.format = FindDepthFormat(deviceInfo);
		if (info.format == VK_FORMAT_UNDEFINED) {
			std::cerr << "Failed to find a supported Vulkan depth format.\n";
			return false;
		}
		if (!CreateImage(deviceInfo, swapChainInfo))
			return false;
		return CreateImageView();
	}

	void VulkanDepthBuffer::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (info.imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device, info.imageView, nullptr);
			info.imageView = VK_NULL_HANDLE;
		}
		if (info.image != VK_NULL_HANDLE) {
			vkDestroyImage(device, info.image, nullptr);
			info.image = VK_NULL_HANDLE;
		}
		if (info.imageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(device, info.imageMemory, nullptr);
			info.imageMemory = VK_NULL_HANDLE;
		}
		info.format = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}

	VkFormat VulkanDepthBuffer::FindDepthFormat(const VulkanDeviceInfo& deviceInfo) {
		constexpr VkFormat candidates[] = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		};
		return FindSupportedFormat(
			deviceInfo,
			candidates,
			std::size(candidates),
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	VkFormat VulkanDepthBuffer::FindSupportedFormat(
		const VulkanDeviceInfo& deviceInfo,
		const VkFormat* candidates,
		const uint32_t candidateCount,
		const VkImageTiling tiling,
		const VkFormatFeatureFlags features) {
		for (uint32_t i = 0; i < candidateCount; i++) {
			VkFormatProperties properties{};
			vkGetPhysicalDeviceFormatProperties(deviceInfo.physicalDevice, candidates[i], &properties);
			if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
				return candidates[i];
			if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
				return candidates[i];
		}
		return VK_FORMAT_UNDEFINED;
	}

	bool VulkanDepthBuffer::FindMemoryType(
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

	bool VulkanDepthBuffer::CreateImage(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo) {
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
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(deviceInfo.device, &imageInfo, nullptr, &info.image) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan depth image.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(deviceInfo.device, info.image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!FindMemoryType(deviceInfo, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			std::cerr << "Failed to find Vulkan depth image memory type.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(deviceInfo.device, &allocateInfo, nullptr, &info.imageMemory) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan depth image memory.\n";
			return false;
		}
		if (vkBindImageMemory(deviceInfo.device, info.image, info.imageMemory, 0) != VK_SUCCESS) {
			std::cerr << "Failed to bind Vulkan depth image memory.\n";
			return false;
		}
		return true;
	}

	bool VulkanDepthBuffer::CreateImageView() {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = info.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = info.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device, &viewInfo, nullptr, &info.imageView) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan depth image view.\n";
			return false;
		}
		return true;
	}
}
