#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

#include <vector>

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
		
		VkCommandBuffer GetCommandBuffer(const uint32_t imageIndex) const { return commandBuffers[imageIndex]; }
		const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return commandBuffers; }

		// 스왑체인 이미지 수에 맞춰 렌더링 명령 버퍼를 할당한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, VkCommandPool sourceCommandPool, const VulkanSwapChainInfo& swapChainInfo);
		// 지정한 스왑체인 이미지에 대한 기본 렌더 패스 명령을 기록한다.
		bool Record(uint32_t imageIndex, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent, const float clearColor[4]) const;
		// 할당한 명령 버퍼를 해제한다.
		void Destroy();
	};
}
