#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	struct VulkanMsaaDepthBufferInfo {
		VkImageView imageView = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	class VulkanMsaaDepthBuffer {
		VulkanMsaaDepthBufferInfo info;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

		// 물리 디바이스에서 사용할 수 있는 depth format을 찾는다.
		static VkFormat FindDepthFormat(const VulkanDeviceInfo& deviceInfo);
		// 후보 format 중 tiling과 feature 조건을 만족하는 format을 찾는다.
		static VkFormat FindSupportedFormat(const VulkanDeviceInfo& deviceInfo, const VkFormat* candidates, uint32_t candidateCount, VkImageTiling tiling, VkFormatFeatureFlags features);
		// 멀티샘플 depth attachment로 사용할 image와 메모리를 생성한다.
		bool CreateImage(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// 멀티샘플 depth image를 framebuffer에 연결하기 위한 image view를 생성한다.
		bool CreateImageView();

	public:
		VulkanMsaaDepthBuffer() = default;
		~VulkanMsaaDepthBuffer();

		VulkanMsaaDepthBuffer(const VulkanMsaaDepthBuffer&) = delete;
		VulkanMsaaDepthBuffer& operator=(const VulkanMsaaDepthBuffer&) = delete;
		VulkanMsaaDepthBuffer(VulkanMsaaDepthBuffer&&) = delete;
		VulkanMsaaDepthBuffer& operator=(VulkanMsaaDepthBuffer&&) = delete;
		
		const VulkanMsaaDepthBufferInfo& GetInfo() const { return info; }
		static bool HasStencilComponent(VkFormat format);

		// 스왑체인 크기에 맞는 멀티샘플 depth image와 image view를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// 생성한 멀티샘플 depth 리소스를 해제한다.
		void Destroy();
	};
}
