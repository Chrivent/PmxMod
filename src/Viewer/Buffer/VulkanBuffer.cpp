#include "Viewer/Buffer/VulkanBuffer.h"

#include "Viewer/Memory/VulkanMemory.h"

namespace Chrivent {
	VulkanBuffer::~VulkanBuffer() {
		Reset();
	}

	GraphicsResult<void> VulkanBuffer::Initialize(
		const VulkanDevice& sourceDevice, const VkDeviceSize bufferSize,
		const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties) {
		Reset();
		device = sourceDevice.GetDevice();
		size = bufferSize;
		if (device == VK_NULL_HANDLE || bufferSize == 0) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "buffer 생성",
				"Vulkan device 또는 buffer 크기가 올바르지 않습니다"));
		}
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "buffer 생성",
				"Vulkan buffer를 만들지 못했습니다", result, true));
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(sourceDevice, memoryRequirements.memoryTypeBits, properties, memoryType)) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::UnsupportedFeature, "buffer memory type 선택",
				"Vulkan buffer에 사용할 memory type을 찾지 못했습니다"));
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "buffer memory 할당",
				"Vulkan buffer memory를 할당하지 못했습니다", result, true));
		}
		result = vkBindBufferMemory(device, buffer, memory, 0);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "buffer memory 연결",
				"Vulkan buffer memory를 연결하지 못했습니다", result, true));
		}
		if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
			result = vkMapMemory(device, memory, 0, bufferSize, 0, &mappedData);
			if (result != VK_SUCCESS) {
				Reset();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "buffer memory 매핑",
					"Vulkan buffer memory를 persistent map하지 못했습니다", result, true));
			}
			persistentlyMapped = true;
		}
		return {};
	}

	bool VulkanBuffer::Write(const void* sourceData, const VkDeviceSize dataSize, const VkDeviceSize offset) const {
		if (device == VK_NULL_HANDLE || memory == VK_NULL_HANDLE || sourceData == nullptr)
			return false;
		if (offset > size || dataSize > size - offset)
			return false;
		if (persistentlyMapped) {
			auto* destination = static_cast<unsigned char*>(mappedData) + offset;
			std::memcpy(destination, sourceData, dataSize);
			return true;
		}
		void* writeTarget = nullptr;
		if (vkMapMemory(device, memory, offset, dataSize, 0, &writeTarget) != VK_SUCCESS)
			return false;
		std::memcpy(writeTarget, sourceData, dataSize);
		vkUnmapMemory(device, memory);
		return true;
	}

	void VulkanBuffer::Reset() {
		if (device != VK_NULL_HANDLE && persistentlyMapped && memory != VK_NULL_HANDLE)
			vkUnmapMemory(device, memory);
		mappedData = nullptr;
		persistentlyMapped = false;
		if (device != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE)
			vkDestroyBuffer(device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
		if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE)
			vkFreeMemory(device, memory, nullptr);
		memory = VK_NULL_HANDLE;
		size = 0;
		device = VK_NULL_HANDLE;
	}
}
