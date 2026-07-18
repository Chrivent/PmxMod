#pragma once

#include "Viewer/Command/VulkanCommandBuffer.h"

namespace Chrivent {
	// Vulkan 명령 풀과 프레임별 명령 버퍼를 관리한다.
	class VulkanCommandContext {
		VkDevice device = VK_NULL_HANDLE;
		VulkanCommandBuffer commandBuffer;
		VkCommandPool commandPool = VK_NULL_HANDLE;

	public:
		VulkanCommandContext() = default;
		~VulkanCommandContext();

		VulkanCommandContext(const VulkanCommandContext&) = delete;
		VulkanCommandContext& operator=(const VulkanCommandContext&) = delete;

		VulkanCommandBuffer& GetCommandBuffer() { return commandBuffer; }
		const VulkanCommandBuffer& GetCommandBuffer() const { return commandBuffer; }
		
		// 그래픽스 큐 패밀리에 맞는 command pool과 command buffer를 생성한다.
		GraphicsResult<void> Initialize(const VulkanDevice& sourceDevice,
			const VulkanSwapChain& sourceSwapChain);
		// 생성한 command pool과 command buffer를 해제한다.
		void Reset();
	};
}
