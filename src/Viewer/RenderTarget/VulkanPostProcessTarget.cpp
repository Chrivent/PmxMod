#include "Viewer/RenderTarget/VulkanPostProcessTarget.h"

#include "Viewer/Memory/VulkanMemory.h"

namespace Chrivent {
	VulkanPostProcessTarget::~VulkanPostProcessTarget() {
		Reset();
	}

	VulkanPostProcessTarget::VulkanPostProcessTarget(VulkanPostProcessTarget&& other) noexcept {
		Swap(other);
	}

	VulkanPostProcessTarget& VulkanPostProcessTarget::operator=(VulkanPostProcessTarget&& other) noexcept {
		if (this != &other) {
			Reset();
			Swap(other);
		}
		return *this;
	}

	bool VulkanPostProcessTarget::CreateImage(const VulkanDevice& sourceDevice, const VkExtent2D extent,
		const VkFormat format, const VkImageUsageFlags usage, const size_t index) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { extent.width, extent.height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(device, &imageInfo, nullptr, &images[index]) != VK_SUCCESS)
			return false;
		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device, images[index], &requirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(
			sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType))
			return false;
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = requirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(device, &allocateInfo, nullptr, &memories[index]) != VK_SUCCESS
			|| vkBindImageMemory(device, images[index], memories[index], 0) != VK_SUCCESS)
			return false;
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = images[index];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		return vkCreateImageView(device, &viewInfo, nullptr, &imageViews[index]) == VK_SUCCESS;
	}

	void VulkanPostProcessTarget::Swap(VulkanPostProcessTarget& other) noexcept {
		std::swap(device, other.device);
		images.swap(other.images);
		memories.swap(other.memories);
		imageViews.swap(other.imageViews);
		initialized.swap(other.initialized);
	}

	bool VulkanPostProcessTarget::Initialize(const VulkanDevice& sourceDevice, const size_t imageCount,
		const VkExtent2D extent, const VkFormat format, const VkImageUsageFlags usage,
		const bool trackInitialization) {
		Reset();
		if (sourceDevice.GetDevice() == VK_NULL_HANDLE || imageCount == 0 || extent.width == 0 || extent.height == 0
			|| format == VK_FORMAT_UNDEFINED)
			return false;
		device = sourceDevice.GetDevice();
		images.resize(imageCount);
		memories.resize(imageCount);
		imageViews.resize(imageCount);
		initialized.assign(trackInitialization ? imageCount : 0, false);
		for (size_t index = 0; index < imageCount; index++) {
			if (!CreateImage(sourceDevice, extent, format, usage, index))
				return false;
		}
		return true;
	}

	void VulkanPostProcessTarget::MarkInitialized(const size_t index) {
		if (index < initialized.size())
			initialized[index] = true;
	}

	void VulkanPostProcessTarget::Reset() {
		if (device != VK_NULL_HANDLE) {
			for (const VkImageView view : imageViews) {
				if (view != VK_NULL_HANDLE)
					vkDestroyImageView(device, view, nullptr);
			}
			for (const VkImage image : images) {
				if (image != VK_NULL_HANDLE)
					vkDestroyImage(device, image, nullptr);
			}
			for (const VkDeviceMemory memory : memories) {
				if (memory != VK_NULL_HANDLE)
					vkFreeMemory(device, memory, nullptr);
			}
		}
		imageViews.clear();
		images.clear();
		memories.clear();
		initialized.clear();
		device = VK_NULL_HANDLE;
	}
}
