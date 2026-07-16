#pragma once

#include "Viewer/Device/VulkanDevice.h"

namespace Chrivent {
	// Vulkan 텍스처 복사에 사용하는 command buffer와 fence를 재사용한다.
	class VulkanTextureUploadContext {
		VkDevice device = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;

		// 이전 디바이스와 command pool에 속한 업로드 객체를 해제한다.
		void Reset();
		// 현재 디바이스와 command pool에서 재사용할 업로드 객체를 준비한다.
		bool Initialize(const VulkanDevice& sourceDevice, VkCommandPool sourceCommandPool);

	public:
		VulkanTextureUploadContext() = default;
		~VulkanTextureUploadContext();
		
		VulkanTextureUploadContext(const VulkanTextureUploadContext&) = delete;
		VulkanTextureUploadContext& operator=(const VulkanTextureUploadContext&) = delete;

		// 텍스처 복사 명령 기록을 시작하고 command buffer를 반환한다.
		bool Begin(const VulkanDevice& sourceDevice, VkCommandPool sourceCommandPool,
			VkCommandBuffer& targetCommandBuffer);
		// 기록한 명령을 제출하고 전용 fence가 완료될 때까지 기다린다.
		bool SubmitAndWait(const VulkanDevice& sourceDevice) const;
	};
}
