#pragma once

#include "VulkanCommandBuffer.h"

namespace Chrivent {
	class VulkanCommandContext {
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanCommandBuffer commandBuffer;
		VkCommandPool commandPool = VK_NULL_HANDLE;

		VulkanCommandContext() = default;
		~VulkanCommandContext();

		VulkanCommandContext(const VulkanCommandContext&) = delete;
		VulkanCommandContext& operator=(const VulkanCommandContext&) = delete;
		VulkanCommandContext(VulkanCommandContext&&) = delete;
		VulkanCommandContext& operator=(VulkanCommandContext&&) = delete;
		
		// 그래픽스 큐 패밀리에 맞는 command pool과 command buffer를 생성한다.
		bool Initialize(const VulkanDevice& deviceInfo, const VulkanSwapChain& swapChainInfo);
		// 생성한 command pool과 command buffer를 해제한다.
		void Destroy();
	};
}
