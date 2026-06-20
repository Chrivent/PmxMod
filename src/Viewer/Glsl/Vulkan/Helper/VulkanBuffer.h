#pragma once

#include "VulkanDevice.h"

namespace Chrivent {
	struct VulkanBufferInfo {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
	};

	class VulkanBuffer {
		VulkanBufferInfo info;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		void* mappedData = nullptr;
		bool persistentlyMapped = false;

	public:
		VulkanBuffer() = default;
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;
		VulkanBuffer(VulkanBuffer&&) = delete;
		VulkanBuffer& operator=(VulkanBuffer&&) = delete;

		const VulkanBufferInfo& GetInfo() const { return info; }
		void* ResolveMappedData() const { return mappedData; }

		// 지정한 크기와 용도에 맞는 Vulkan buffer와 메모리를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		// host visible 메모리에 데이터를 복사한다.
		bool Write(const void* sourceData, VkDeviceSize dataSize, VkDeviceSize offset = 0) const;
		// 생성한 buffer와 메모리를 해제한다.
		void Destroy();
	};
}
