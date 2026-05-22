#pragma once

#include "VulkanCommandBuffer.h"

namespace Chrivent {
	class VulkanCommandContext {
		VulkanCommandBuffer commandBuffer;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanCommandContext() = default;
		~VulkanCommandContext();

		VulkanCommandContext(const VulkanCommandContext&) = delete;
		VulkanCommandContext& operator=(const VulkanCommandContext&) = delete;
		VulkanCommandContext(VulkanCommandContext&&) = delete;
		VulkanCommandContext& operator=(VulkanCommandContext&&) = delete;
		
		VkCommandPool GetCommandPool() const { return commandPool; }
		const VulkanCommandBuffer& GetCommandBuffer() const { return commandBuffer; }

		// 洹몃옒?쎌뒪 ???⑤?由ъ뿉 留욌뒗 command pool怨?command buffer瑜??앹꽦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// ?앹꽦??command pool怨?command buffer瑜??댁젣?쒕떎.
		void Destroy();
	};
}
