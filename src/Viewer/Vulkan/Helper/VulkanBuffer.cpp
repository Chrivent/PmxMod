#include "VulkanBuffer.h"

#include <iostream>

namespace Chrivent {
	VulkanBuffer::~VulkanBuffer() {
		Destroy();
	}

	bool VulkanBuffer::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VkDeviceSize size,
		const VkBufferUsageFlags usage,
		const VkMemoryPropertyFlags properties) {
		Destroy();
		device = deviceInfo.device;
		info.size = size;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(deviceInfo.device, &bufferInfo, nullptr, &info.buffer) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan buffer.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(deviceInfo.device, info.buffer, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!FindMemoryType(deviceInfo, memoryRequirements.memoryTypeBits, properties, memoryType)) {
			std::cerr << "Failed to find Vulkan buffer memory type.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(deviceInfo.device, &allocateInfo, nullptr, &info.memory) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan buffer memory.\n";
			return false;
		}
		if (vkBindBufferMemory(deviceInfo.device, info.buffer, info.memory, 0) != VK_SUCCESS) {
			std::cerr << "Failed to bind Vulkan buffer memory.\n";
			return false;
		}
		return true;
	}

	bool VulkanBuffer::Write(const void* sourceData, const VkDeviceSize dataSize, const VkDeviceSize offset) const {
		if (device == VK_NULL_HANDLE || info.memory == VK_NULL_HANDLE || sourceData == nullptr)
			return false;
		if (offset + dataSize > info.size) {
			std::cerr << "Failed to write Vulkan buffer: source data is larger than buffer.\n";
			return false;
		}
		void* mappedData = nullptr;
		if (vkMapMemory(device, info.memory, offset, dataSize, 0, &mappedData) != VK_SUCCESS) {
			std::cerr << "Failed to map Vulkan buffer memory.\n";
			return false;
		}
		std::memcpy(mappedData, sourceData, dataSize);
		vkUnmapMemory(device, info.memory);
		return true;
	}

	void VulkanBuffer::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (info.buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(device, info.buffer, nullptr);
			info.buffer = VK_NULL_HANDLE;
		}
		if (info.memory != VK_NULL_HANDLE) {
			vkFreeMemory(device, info.memory, nullptr);
			info.memory = VK_NULL_HANDLE;
		}
		info.size = 0;
		device = VK_NULL_HANDLE;
	}

	bool VulkanBuffer::FindMemoryType(
		const VulkanDeviceInfo& deviceInfo,
		const uint32_t typeFilter,
		const VkMemoryPropertyFlags properties,
		uint32_t& memoryType) {
		VkPhysicalDeviceMemoryProperties memoryProperties{};
		vkGetPhysicalDeviceMemoryProperties(deviceInfo.physicalDevice, &memoryProperties);
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
			if ((typeFilter & 1 << i) != 0 &&
				(memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				memoryType = i;
				return true;
			}
		}
		return false;
	}
}
