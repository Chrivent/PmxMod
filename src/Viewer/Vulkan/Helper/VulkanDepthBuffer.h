#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	struct VulkanDepthBufferInfo {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	class VulkanDepthBuffer {
		VulkanDepthBufferInfo info;
		VkDevice device = VK_NULL_HANDLE;

		// 臾쇰━ ?붾컮?댁뒪?먯꽌 ?ъ슜?????덈뒗 depth format??李얜뒗??
		static VkFormat FindDepthFormat(const VulkanDeviceInfo& deviceInfo);
		// ?꾨낫 format 以?tiling怨?feature 議곌굔??留뚯”?섎뒗 format??李얜뒗??
		static VkFormat FindSupportedFormat(const VulkanDeviceInfo& deviceInfo, const VkFormat* candidates, uint32_t candidateCount, VkImageTiling tiling, VkFormatFeatureFlags features);
		// 硫붾え由??붽뎄 議곌굔??留욌뒗 Vulkan memory type index瑜?李얜뒗??
		static bool FindMemoryType(const VulkanDeviceInfo& deviceInfo, uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& memoryType);
		// depth attachment濡??ъ슜??image? 硫붾え由щ? ?앹꽦?쒕떎.
		bool CreateImage(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// depth image瑜?framebuffer???곌껐?섍린 ?꾪븳 image view瑜??앹꽦?쒕떎.
		bool CreateImageView();

	public:
		VulkanDepthBuffer() = default;
		~VulkanDepthBuffer();

		VulkanDepthBuffer(const VulkanDepthBuffer&) = delete;
		VulkanDepthBuffer& operator=(const VulkanDepthBuffer&) = delete;
		VulkanDepthBuffer(VulkanDepthBuffer&&) = delete;
		VulkanDepthBuffer& operator=(VulkanDepthBuffer&&) = delete;
		
		VulkanDepthBufferInfo& GetInfo() { return info; }
		const VulkanDepthBufferInfo& GetInfo() const { return info; }

		// ?ㅼ솑泥댁씤 ?ш린??留욌뒗 depth image? image view瑜??앹꽦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// ?앹꽦??depth 由ъ냼?ㅻ? ?댁젣?쒕떎.
		void Destroy();
	};
}
