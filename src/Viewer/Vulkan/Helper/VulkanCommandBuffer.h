#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	class VulkanCommandBuffer {
		std::vector<VkCommandBuffer> commandBuffers;
		VkDevice device = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;

	public:
		VulkanCommandBuffer() = default;
		~VulkanCommandBuffer();

		VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
		VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;
		VulkanCommandBuffer(VulkanCommandBuffer&&) = delete;
		VulkanCommandBuffer& operator=(VulkanCommandBuffer&&) = delete;
		
		const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return commandBuffers; }

		// ?ㅼ솑泥댁씤 ?대?吏 ?섏뿉 留욎떠 ?뚮뜑留?紐낅졊 踰꾪띁瑜??좊떦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, VkCommandPool sourceCommandPool, const VulkanSwapChainInfo& swapChainInfo);
		// ?좊떦??紐낅졊 踰꾪띁瑜??댁젣?쒕떎.
		void Destroy();
	};
}
