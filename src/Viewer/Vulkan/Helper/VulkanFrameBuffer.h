#pragma once

#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"

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
		
		// 스왑체인 image view마다 멀티샘플 color, depth, resolve attachment를 묶은 framebuffer를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkRenderPass renderPass, VkImageView colorImageView, VkImageView depthImageView);
		// 별도 장면 resolve image view마다 멀티샘플 color/depth attachment를 묶은 framebuffer를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkRenderPass renderPass, VkImageView colorImageView, VkImageView depthImageView,
			const std::vector<VkImageView>& resolveImageViews);
		// 생성한 framebuffer 리소스를 해제한다.
		void Destroy();
	};
}
