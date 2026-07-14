#pragma once

#include "Viewer/Device/VulkanDevice.h"

namespace Chrivent {
	class VulkanBuffer {
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		void* mappedData = nullptr;
		bool persistentlyMapped = false;

	public:
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceSize size = 0;

		VulkanBuffer() = default;
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;
		VulkanBuffer(VulkanBuffer&&) = delete;
		VulkanBuffer& operator=(VulkanBuffer&&) = delete;

		void* ResolveMappedData() const { return mappedData; }

		// 지정한 크기와 용도에 맞는 Vulkan buffer와 메모리를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		// host visible 메모리에 데이터를 복사한다.
		bool Write(const void* sourceData, VkDeviceSize dataSize, VkDeviceSize offset = 0) const;
		// 생성한 buffer와 메모리를 해제한다.
		void Reset();
	};
}
