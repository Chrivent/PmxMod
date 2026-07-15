#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"

#include <span>

namespace Chrivent {
	// Vulkan 장면 렌더링용 MSAA depth image와 view를 관리한다.
	class VulkanMsaaDepthBuffer {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

		// 물리 디바이스에서 사용할 수 있는 depth format을 찾는다.
		static VkFormat FindDepthFormat(const VulkanDevice& sourceDevice);
		// 후보 format 중 tiling과 feature 조건을 만족하는 format을 찾는다.
		static VkFormat FindSupportedFormat(const VulkanDevice& sourceDevice, std::span<const VkFormat> candidates,
			VkImageTiling tiling, VkFormatFeatureFlags features);
		// 멀티샘플 depth attachment로 사용할 image와 메모리를 생성한다.
		bool CreateImage(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 멀티샘플 depth image를 framebuffer에 연결하기 위한 image view를 생성한다.
		bool CreateImageView();

	public:
		VkImageView imageView = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImage GetImage() const { return image; }

		VulkanMsaaDepthBuffer() = default;
		~VulkanMsaaDepthBuffer();

		VulkanMsaaDepthBuffer(const VulkanMsaaDepthBuffer&) = delete;
		VulkanMsaaDepthBuffer& operator=(const VulkanMsaaDepthBuffer&) = delete;
		
		// depth format에 stencil 성분이 포함되어 있는지 확인한다.
		static bool HasStencilComponent(const VkFormat format) {
			return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
		}

		// 스왑체인 크기에 맞는 멀티샘플 depth image와 image view를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 생성한 멀티샘플 depth 리소스를 해제한다.
		void Reset();
	};
}
