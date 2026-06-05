#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

namespace Chrivent {
	struct VulkanMsaaColorBufferInfo {
		VkImageView imageView = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	class VulkanMsaaColorBuffer {
		VulkanMsaaColorBufferInfo info;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

		// 멀티샘플 color attachment image와 메모리를 생성한다.
		bool CreateImage(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// 멀티샘플 color image용 image view를 생성한다.
		bool CreateImageView();

	public:
		VulkanMsaaColorBuffer() = default;
		~VulkanMsaaColorBuffer();

		VulkanMsaaColorBuffer(const VulkanMsaaColorBuffer&) = delete;
		VulkanMsaaColorBuffer& operator=(const VulkanMsaaColorBuffer&) = delete;
		VulkanMsaaColorBuffer(VulkanMsaaColorBuffer&&) = delete;
		VulkanMsaaColorBuffer& operator=(VulkanMsaaColorBuffer&&) = delete;

		const VulkanMsaaColorBufferInfo& GetInfo() const { return info; }

		// 스왑체인 크기에 맞는 멀티샘플 color image와 image view를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo);
		// 생성한 멀티샘플 color 리소스를 해제한다.
		void Destroy();
	};
}
