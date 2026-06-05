#pragma once

#include "VulkanDevice.h"

namespace Chrivent {
	class VulkanMemory {
	public:
		// 물리 디바이스 메모리 중 요청한 속성을 만족하는 memory type index를 찾는다.
		static bool FindMemoryType(
			const VulkanDeviceInfo& deviceInfo,
			uint32_t typeFilter,
			VkMemoryPropertyFlags properties,
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
	};
}
