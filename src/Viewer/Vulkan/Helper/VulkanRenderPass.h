#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	class VulkanRenderPass {
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanRenderPass() = default;
		~VulkanRenderPass();

		VulkanRenderPass(const VulkanRenderPass&) = delete;
		VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
		VulkanRenderPass(VulkanRenderPass&&) = delete;
		VulkanRenderPass& operator=(VulkanRenderPass&&) = delete;
		
		VkRenderPass GetRenderPass() const { return renderPass; }

		// ?ㅼ솑泥댁씤 color format怨?depth format??留욌뒗 render pass瑜??앹꽦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo, VkFormat depthFormat);
		// ?앹꽦??render pass 由ъ냼?ㅻ? ?댁젣?쒕떎.
		void Destroy();
	};
}
