#include "Viewer/Buffer/VulkanBuffer.h"

#include "Viewer/Memory/VulkanMemory.h"

#include <iostream>

namespace Chrivent {
	VulkanBuffer::~VulkanBuffer() {
		Reset();
	}

	bool VulkanBuffer::Initialize(const VulkanDevice& sourceDevice, const VkDeviceSize bufferSize,
		const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties) {
		Reset();
		device = sourceDevice.GetDevice();
		size = bufferSize;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(sourceDevice.GetDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			std::cerr << "Vulkan buffer를 만들지 못했습니다.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(sourceDevice.GetDevice(), buffer, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(sourceDevice, memoryRequirements.memoryTypeBits, properties, memoryType)) {
			std::cerr << "Vulkan buffer memory type을 찾지 못했습니다.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(sourceDevice.GetDevice(), &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
			std::cerr << "Vulkan buffer memory를 할당하지 못했습니다.\n";
			return false;
		}
		if (vkBindBufferMemory(sourceDevice.GetDevice(), buffer, memory, 0) != VK_SUCCESS) {
			std::cerr << "Vulkan buffer memory를 연결하지 못했습니다.\n";
			return false;
		}
		if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
			if (vkMapMemory(sourceDevice.GetDevice(), memory, 0, bufferSize, 0, &mappedData) != VK_SUCCESS) {
				std::cerr << "Vulkan buffer memory를 persistent map하지 못했습니다.\n";
				return false;
			}
			persistentlyMapped = true;
		}
		return true;
	}

	bool VulkanBuffer::Write(const void* sourceData, const VkDeviceSize dataSize, const VkDeviceSize offset) const {
		if (device == VK_NULL_HANDLE || memory == VK_NULL_HANDLE || sourceData == nullptr)
			return false;
		if (offset > size || dataSize > size - offset) {
			std::cerr << "Vulkan buffer에 쓸 원본 데이터가 buffer보다 큽니다.\n";
			return false;
		}
		if (persistentlyMapped) {
			auto* destination = static_cast<unsigned char*>(mappedData) + offset;
			std::memcpy(destination, sourceData, dataSize);
			return true;
		}
		void* writeTarget = nullptr;
		if (vkMapMemory(device, memory, offset, dataSize, 0, &writeTarget) != VK_SUCCESS) {
			std::cerr << "Vulkan buffer memory를 map하지 못했습니다.\n";
			return false;
		}
		std::memcpy(writeTarget, sourceData, dataSize);
		vkUnmapMemory(device, memory);
		return true;
	}

	void VulkanBuffer::Reset() {
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
