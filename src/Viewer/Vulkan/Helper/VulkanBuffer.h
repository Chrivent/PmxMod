#pragma once

#include "VulkanDevice.h"

namespace Chrivent {
	struct VulkanBufferInfo {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
	};

	class VulkanBuffer {
		VulkanBufferInfo info;
		VkDevice device = VK_NULL_HANDLE;
		void* mappedData = nullptr;
		bool persistentlyMapped = false;

		// 물리 디바이스 메모리 중 요청한 속성을 만족하는 memory type index를 찾는다.
		static bool FindMemoryType(const VulkanDeviceInfo& deviceInfo, uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& memoryType);

	public:
		VulkanBuffer() = default;
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;
		VulkanBuffer(VulkanBuffer&&) = delete;
		VulkanBuffer& operator=(VulkanBuffer&&) = delete;

		VulkanBufferInfo& GetInfo() { return info; }
		const VulkanBufferInfo& GetInfo() const { return info; }

		// 지정한 크기와 용도에 맞는 Vulkan buffer와 메모리를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		// host visible 메모리에 데이터를 복사한다.
		bool Write(const void* sourceData, VkDeviceSize dataSize, VkDeviceSize offset = 0) const;
		// 생성한 buffer와 메모리를 해제한다.
		void Destroy();
	};
}
