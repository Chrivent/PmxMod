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
		VulkanCommandBuffer& GetCommandBuffer() { return commandBuffer; }
		const VulkanCommandBuffer& GetCommandBuffer() const { return commandBuffer; }
		
		// 그래픽스 큐 패밀리에 맞는 command pool과 command buffer를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// 생성한 command pool과 command buffer를 해제한다.
		void Destroy();
	};
}
