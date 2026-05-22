#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	class VulkanFrameBuffer {
		std::vector<VkFramebuffer> frameBuffers;
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanFrameBuffer() = default;
		~VulkanFrameBuffer();

		VulkanFrameBuffer(const VulkanFrameBuffer&) = delete;
		VulkanFrameBuffer& operator=(const VulkanFrameBuffer&) = delete;
		VulkanFrameBuffer(VulkanFrameBuffer&&) = delete;
		VulkanFrameBuffer& operator=(VulkanFrameBuffer&&) = delete;
		
		const std::vector<VkFramebuffer>& GetFrameBuffers() const { return frameBuffers; }

		// 스왑체인 image view마다 렌더링에 사용할 framebuffer를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo, VkRenderPass renderPass);
		// 생성한 framebuffer 리소스를 해제한다.
		void Destroy();
	};
}
