#pragma once

#include "Viewer/Device/VulkanDevice.h"

namespace Chrivent {
	// Vulkan 버퍼, 메모리와 선택적인 매핑 주소를 소유한다.
	class VulkanBuffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		void* mappedData = nullptr;
		bool persistentlyMapped = false;
		VkDeviceSize size = 0;

	public:
		VulkanBuffer() = default;
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;

		VkBuffer GetBuffer() const { return buffer; }
		void* GetMappedData() const { return mappedData; }

		// 지정한 크기와 용도에 맞는 Vulkan buffer와 메모리를 생성한다.
		GraphicsError::Result<void> Initialize(const VulkanDevice& sourceDevice, VkDeviceSize bufferSize,
			VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		// host visible 메모리에 데이터를 복사한다.
		bool Write(const void* sourceData, VkDeviceSize dataSize, VkDeviceSize offset = 0) const;
		// 생성한 buffer와 메모리를 해제한다.
		void Reset();
	};
}
