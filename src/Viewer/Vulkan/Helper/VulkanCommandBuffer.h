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
		
		VkCommandBuffer GetCommandBuffer(uint32_t imageIndex) const { return commandBuffers[imageIndex]; }
		const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return commandBuffers; }

		// ?ㅼ솑泥댁씤 ?대?吏 ?섏뿉 留욎떠 ?뚮뜑留?紐낅졊 踰꾪띁瑜??좊떦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, VkCommandPool sourceCommandPool, const VulkanSwapChainInfo& swapChainInfo);
		// 吏?뺥븳 ?ㅼ솑泥댁씤 ?대?吏?????湲곕낯 ?뚮뜑 ?⑥뒪 紐낅졊??湲곕줉?쒕떎.
		bool Record(uint32_t imageIndex, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent, const float clearColor[4]) const;
		// ?좊떦??紐낅졊 踰꾪띁瑜??댁젣?쒕떎.
		void Destroy();
	};
}
