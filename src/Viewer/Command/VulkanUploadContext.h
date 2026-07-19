#pragma once

#include "Viewer/Device/VulkanDevice.h"

#include <memory>
#include <vector>

namespace Chrivent {
	class VulkanBuffer;

	// Vulkan 정적 GPU 리소스 복사에 사용하는 command buffer와 fence를 재사용한다.
	class VulkanUploadContext {
		VkDevice device = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		std::vector<std::unique_ptr<VulkanBuffer>> retainedBuffers;
		bool batchRecording = false;

		// 이전 디바이스에 속한 업로드 command pool과 동기화 객체를 해제한다.
		void Reset();
		// 현재 디바이스에서 재사용할 전용 업로드 command pool과 객체를 준비한다.
		GraphicsError::Result<void> Initialize(const VulkanDevice& sourceDevice);
		// 기록한 명령을 제출하고 전용 fence가 완료될 때까지 기다린다.
		GraphicsError::Result<void> SubmitAndWait(const VulkanDevice& sourceDevice) const;

	public:
		VulkanUploadContext() = default;
		~VulkanUploadContext();

		VulkanUploadContext(const VulkanUploadContext&) = delete;
		VulkanUploadContext& operator=(const VulkanUploadContext&) = delete;

		// 여러 정적 리소스 복사를 기록할 batch를 시작하고 command buffer를 반환한다.
		GraphicsError::Result<void> BeginBatch(const VulkanDevice& sourceDevice,
			VkCommandBuffer& targetCommandBuffer);
		// 기록한 batch를 한 번 제출하고 GPU 완료까지 기다린다.
		GraphicsError::Result<void> SubmitBatch(const VulkanDevice& sourceDevice);
		// 현재 batch가 참조하는 staging buffer의 수명을 제출 완료까지 유지한다.
		GraphicsError::Result<void> RetainStagingBuffer(std::unique_ptr<VulkanBuffer> stagingBuffer);
		// 현재 batch에 index buffer 복사와 index 입력 동기화를 기록한다.
		GraphicsError::Result<void> RecordIndexBufferUpload(
			VkBuffer destination, std::unique_ptr<VulkanBuffer> source, VkDeviceSize size);
		// 제출하지 않은 현재 batch와 staging buffer를 폐기한다.
		void CancelBatch();
	};
}
