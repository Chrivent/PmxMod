#include "VulkanBuffer.h"

#include "VulkanMemory.h"

#include <iostream>

namespace Chrivent {
	VulkanBuffer::~VulkanBuffer() {
		Destroy();
	}

	bool VulkanBuffer::Initialize(const VulkanDevice& deviceInfo, const VkDeviceSize bufferSize,
		const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties) {
		Destroy();
		device = deviceInfo.device;
		size = bufferSize;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(deviceInfo.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan buffer.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(deviceInfo.device, buffer, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(deviceInfo, memoryRequirements.memoryTypeBits, properties, memoryType)) {
			std::cerr << "Failed to find Vulkan buffer memory type.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(deviceInfo.device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan buffer memory.\n";
			return false;
		}
		if (vkBindBufferMemory(deviceInfo.device, buffer, memory, 0) != VK_SUCCESS) {
			std::cerr << "Failed to bind Vulkan buffer memory.\n";
			return false;
		}
		if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
			if (vkMapMemory(deviceInfo.device, memory, 0, bufferSize, 0, &mappedData) != VK_SUCCESS) {
				std::cerr << "Failed to persistently map Vulkan buffer memory.\n";
				return false;
			}
			persistentlyMapped = true;
		}
		return true;
	}

	bool VulkanBuffer::Write(const void* sourceData, const VkDeviceSize dataSize, const VkDeviceSize offset) const {
		if (device == VK_NULL_HANDLE || memory == VK_NULL_HANDLE || sourceData == nullptr)
			return false;
		if (offset + dataSize > size) {
			std::cerr << "Failed to write Vulkan buffer: source data is larger than buffer.\n";
			return false;
		}
		if (persistentlyMapped) {
			auto* destination = static_cast<unsigned char*>(mappedData) + offset;
			std::memcpy(destination, sourceData, dataSize);
			return true;
		}
		void* writeTarget = nullptr;
		if (vkMapMemory(device, memory, offset, dataSize, 0, &writeTarget) != VK_SUCCESS) {
			std::cerr << "Failed to map Vulkan buffer memory.\n";
			return false;
		}
		std::memcpy(writeTarget, sourceData, dataSize);
		vkUnmapMemory(device, memory);
		return true;
	}

	void VulkanBuffer::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (persistentlyMapped && memory != VK_NULL_HANDLE)
			vkUnmapMemory(device, memory);
		mappedData = nullptr;
		persistentlyMapped = false;
		if (buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(device, buffer, nullptr);
			buffer = VK_NULL_HANDLE;
		}
		if (memory != VK_NULL_HANDLE) {
			vkFreeMemory(device, memory, nullptr);
			memory = VK_NULL_HANDLE;
		}
		size = 0;
		device = VK_NULL_HANDLE;
	}
}
