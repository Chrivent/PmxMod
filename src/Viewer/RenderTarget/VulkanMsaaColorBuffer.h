#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"

namespace Chrivent {
	// Vulkan 장면 렌더링용 MSAA 색상 image와 view를 관리한다.
	class VulkanMsaaColorBuffer {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

		// 멀티샘플 color attachment image와 메모리를 생성한다.
		bool CreateImage(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 멀티샘플 color image용 image view를 생성한다.
		bool CreateImageView();

	public:
		VkImageView imageView = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImage GetImage() const { return image; }

		VulkanMsaaColorBuffer() = default;
		~VulkanMsaaColorBuffer();

		VulkanMsaaColorBuffer(const VulkanMsaaColorBuffer&) = delete;
		VulkanMsaaColorBuffer& operator=(const VulkanMsaaColorBuffer&) = delete;
		VulkanMsaaColorBuffer(VulkanMsaaColorBuffer&&) = delete;
		VulkanMsaaColorBuffer& operator=(VulkanMsaaColorBuffer&&) = delete;

		// 스왑체인 크기에 맞는 멀티샘플 color image와 image view를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 생성한 멀티샘플 color 리소스를 해제한다.
		void Reset();
	};
}
