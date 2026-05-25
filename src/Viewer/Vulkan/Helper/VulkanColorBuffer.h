#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	struct VulkanColorBufferInfo {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	class VulkanColorBuffer {
		VulkanColorBufferInfo info;
		VkDevice device = VK_NULL_HANDLE;

		// Finds a Vulkan memory type index that matches the requested properties.
		static bool FindMemoryType(const VulkanDeviceInfo& deviceInfo, uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& memoryType);
		// Creates the multisampled color attachment image and its memory.
		bool CreateImage(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// Creates an image view for the multisampled color image.
		bool CreateImageView();

	public:
		VulkanColorBuffer() = default;
		~VulkanColorBuffer();

		VulkanColorBuffer(const VulkanColorBuffer&) = delete;
		VulkanColorBuffer& operator=(const VulkanColorBuffer&) = delete;
		VulkanColorBuffer(VulkanColorBuffer&&) = delete;
		VulkanColorBuffer& operator=(VulkanColorBuffer&&) = delete;

		const VulkanColorBufferInfo& GetInfo() const { return info; }

		// Creates the multisampled color image and image view for the swapchain.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// Releases the multisampled color resources.
		void Destroy();
	};
}
