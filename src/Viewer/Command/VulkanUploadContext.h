#pragma once

#include "Viewer/Device/VulkanDevice.h"

namespace Chrivent {
	// Vulkan 정적 GPU 리소스 복사에 사용하는 command buffer와 fence를 재사용한다.
	class VulkanUploadContext {
		VkDevice device = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;

		// 이전 디바이스에 속한 업로드 command pool과 동기화 객체를 해제한다.
		void Reset();
		// 현재 디바이스에서 재사용할 전용 업로드 command pool과 객체를 준비한다.
		bool Initialize(const VulkanDevice& sourceDevice);

	public:
		VulkanUploadContext() = default;
		~VulkanUploadContext();

		VulkanUploadContext(const VulkanUploadContext&) = delete;
		VulkanUploadContext& operator=(const VulkanUploadContext&) = delete;

		// 리소스 복사 명령 기록을 시작하고 command buffer를 반환한다.
		bool Begin(const VulkanDevice& sourceDevice, VkCommandBuffer& targetCommandBuffer);
		// 기록한 명령을 제출하고 전용 fence가 완료될 때까지 기다린다.
		bool SubmitAndWait(const VulkanDevice& sourceDevice) const;
		// staging buffer를 정적 GPU index buffer에 복사한다.
		bool UploadIndexBuffer(const VulkanDevice& sourceDevice,
			VkBuffer destination, VkBuffer source, VkDeviceSize size);
	};
}
