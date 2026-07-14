#include "Viewer/RenderTarget/VulkanMsaaColorBuffer.h"

#include "Viewer/Memory/VulkanMemory.h"

#include <iostream>

namespace Chrivent {
	bool VulkanMsaaColorBuffer::CreateImage(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = sourceSwapChain.extent.width;
		imageInfo.extent.height = sourceSwapChain.extent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.samples = sourceDevice.msaaSampleCount;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(sourceDevice.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan MSAA color image.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(sourceDevice.device, image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(sourceDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			std::cerr << "Failed to find Vulkan MSAA color image memory type.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(sourceDevice.device, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan MSAA color image memory.\n";
			return false;
		}
		if (vkBindImageMemory(sourceDevice.device, image, imageMemory, 0) != VK_SUCCESS) {
			std::cerr << "Failed to bind Vulkan MSAA color image memory.\n";
			return false;
		}
		return true;
	}

	bool VulkanMsaaColorBuffer::CreateImageView() {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan MSAA color image view.\n";
			return false;
		}
		return true;
	}

	VulkanMsaaColorBuffer::~VulkanMsaaColorBuffer() {
		Reset();
	}

	bool VulkanMsaaColorBuffer::Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		device = sourceDevice.device;
		format = sourceSwapChain.imageFormat;
		if (!CreateImage(sourceDevice, sourceSwapChain))
			return false;
		return CreateImageView();
	}

	void VulkanMsaaColorBuffer::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		if (imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device, imageView, nullptr);
			imageView = VK_NULL_HANDLE;
		}
		if (image != VK_NULL_HANDLE) {
			vkDestroyImage(device, image, nullptr);
			image = VK_NULL_HANDLE;
		}
		if (imageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(device, imageMemory, nullptr);
			imageMemory = VK_NULL_HANDLE;
		}
		format = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}
}
