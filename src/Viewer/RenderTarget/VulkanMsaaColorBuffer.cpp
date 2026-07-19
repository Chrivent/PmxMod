#include "Viewer/RenderTarget/VulkanMsaaColorBuffer.h"

#include "Viewer/Memory/VulkanMemory.h"

namespace Chrivent {
	GraphicsError::Result<void> VulkanMsaaColorBuffer::CreateImage(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = sourceSwapChain.GetExtent().width;
		imageInfo.extent.height = sourceSwapChain.GetExtent().height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.samples = sourceDevice.GetMsaaSampleCount();
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult result = vkCreateImage(sourceDevice.GetDevice(), &imageInfo, nullptr, &image);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "MSAA color image 생성",
				"Vulkan MSAA color image를 만들지 못했습니다", result, true));
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(sourceDevice.GetDevice(), image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(sourceDevice, memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::UnsupportedFeature, "MSAA color memory type 선택",
				"Vulkan MSAA color image에 사용할 memory type을 찾지 못했습니다"));
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		result = vkAllocateMemory(sourceDevice.GetDevice(), &allocateInfo, nullptr, &imageMemory);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "MSAA color memory 할당",
				"Vulkan MSAA color image memory를 할당하지 못했습니다", result, true));
		}
		result = vkBindImageMemory(sourceDevice.GetDevice(), image, imageMemory, 0);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "MSAA color memory 연결",
				"Vulkan MSAA color image memory를 연결하지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanMsaaColorBuffer::CreateImageView() {
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
		const VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "MSAA color image view 생성",
				"Vulkan MSAA color image view를 만들지 못했습니다", result, true));
		}
		return {};
	}

	VulkanMsaaColorBuffer::~VulkanMsaaColorBuffer() {
		Reset();
	}

	GraphicsError::Result<void> VulkanMsaaColorBuffer::Initialize(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		Reset();
		device = sourceDevice.GetDevice();
		format = sourceSwapChain.GetImageFormat();
		if (device == VK_NULL_HANDLE || format == VK_FORMAT_UNDEFINED
			|| sourceSwapChain.GetExtent().width == 0 || sourceSwapChain.GetExtent().height == 0) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "MSAA color buffer 초기화",
				"Vulkan device, color format 또는 출력 크기가 올바르지 않습니다"));
		}
		auto result = CreateImage(sourceDevice, sourceSwapChain);
		if (result)
			result = CreateImageView();
		if (result)
			return {};
		const GraphicsError error = result.error();
		Reset();
		return std::unexpected(error);
	}

	void VulkanMsaaColorBuffer::Reset() {
		if (device != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE)
			vkDestroyImageView(device, imageView, nullptr);
		imageView = VK_NULL_HANDLE;
		if (device != VK_NULL_HANDLE && image != VK_NULL_HANDLE)
			vkDestroyImage(device, image, nullptr);
		image = VK_NULL_HANDLE;
		if (device != VK_NULL_HANDLE && imageMemory != VK_NULL_HANDLE)
			vkFreeMemory(device, imageMemory, nullptr);
		imageMemory = VK_NULL_HANDLE;
		format = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}
}
