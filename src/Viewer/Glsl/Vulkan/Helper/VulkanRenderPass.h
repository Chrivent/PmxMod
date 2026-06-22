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

		// 스왑체인 color format, 멀티샘플 color/depth, resolve attachment에 맞는 render pass를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain, VkFormat depthFormat);
		// 생성한 render pass 리소스를 해제한다.
		void Destroy();
	};
}
