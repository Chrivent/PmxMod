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

	GraphicsResult<void> VulkanPostProcessTarget::CreateImage(
		const VulkanDevice& sourceDevice, const VkExtent2D extent,
		const VkFormat format, const VkImageUsageFlags usage,
		const VkImageAspectFlags aspectMask, const size_t index) {
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
		VkResult result = vkCreateImage(device, &imageInfo, nullptr, &images[index]);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 image 생성",
				"Vulkan 후처리 image를 만들지 못했습니다", result, true));
		}
		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device, images[index], &requirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(
			sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::UnsupportedFeature, "후처리 image memory type 선택",
				"Vulkan 후처리 image에 사용할 memory type을 찾지 못했습니다"));
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = requirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		result = vkAllocateMemory(device, &allocateInfo, nullptr, &memories[index]);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 image memory 할당",
				"Vulkan 후처리 image memory를 할당하지 못했습니다", result, true));
		}
		result = vkBindImageMemory(device, images[index], memories[index], 0);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 image memory 연결",
				"Vulkan 후처리 image memory를 연결하지 못했습니다", result, true));
		}
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = images[index];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectMask;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		result = vkCreateImageView(device, &viewInfo, nullptr, &imageViews[index]);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 image view 생성",
				"Vulkan 후처리 image view를 만들지 못했습니다", result, true));
		}
		return {};
	}

	void VulkanPostProcessTarget::Swap(VulkanPostProcessTarget& other) noexcept {
		std::swap(device, other.device);
		images.swap(other.images);
		memories.swap(other.memories);
		imageViews.swap(other.imageViews);
		initialized.swap(other.initialized);
		pendingInitialized.swap(other.pendingInitialized);
		std::swap(initializationFramePending, other.initializationFramePending);
	}

	GraphicsResult<void> VulkanPostProcessTarget::Initialize(
		const VulkanDevice& sourceDevice, const size_t imageCount,
		const VkExtent2D extent, const VkFormat format, const VkImageUsageFlags usage,
		const bool trackInitialization, const VkImageAspectFlags aspectMask) {
		Reset();
		if (sourceDevice.GetDevice() == VK_NULL_HANDLE || imageCount == 0 || extent.width == 0 || extent.height == 0
			|| format == VK_FORMAT_UNDEFINED || aspectMask == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "후처리 target 초기화",
				"Vulkan device, image 수, format, extent 또는 aspect가 올바르지 않습니다"));
		}
		device = sourceDevice.GetDevice();
		images.resize(imageCount);
		memories.resize(imageCount);
		imageViews.resize(imageCount);
		initialized.assign(trackInitialization ? imageCount : 0, false);
		for (size_t index = 0; index < imageCount; index++) {
			const auto result = CreateImage(sourceDevice, extent, format, usage, aspectMask, index);
			if (!result) {
				const GraphicsError error = result.error();
				Reset();
				return std::unexpected(error);
			}
		}
		return {};
	}

	void VulkanPostProcessTarget::BeginInitializationFrame() {
		if (initializationFramePending || initialized.empty())
			return;
		pendingInitialized = initialized;
		initializationFramePending = true;
	}

	void VulkanPostProcessTarget::MarkInitialized(const size_t index) {
		auto& states = initializationFramePending ? pendingInitialized : initialized;
		if (index < states.size())
			states[index] = true;
	}

	void VulkanPostProcessTarget::CommitInitializationFrame() {
		if (!initializationFramePending)
			return;
		initialized.swap(pendingInitialized);
		pendingInitialized.clear();
		initializationFramePending = false;
	}

	void VulkanPostProcessTarget::DiscardInitializationFrame() {
		pendingInitialized.clear();
		initializationFramePending = false;
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
		pendingInitialized.clear();
		initializationFramePending = false;
		device = VK_NULL_HANDLE;
	}
}
