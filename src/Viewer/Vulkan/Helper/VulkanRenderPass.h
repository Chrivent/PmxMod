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

		// 스왑체인 이미지 포맷에 맞는 기본 color render pass를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// 생성한 render pass 리소스를 해제한다.
		void Destroy();

		VkRenderPass GetRenderPass() const { return renderPass; }
	};
}
