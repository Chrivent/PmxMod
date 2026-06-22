#pragma once

#include "VulkanDevice.h"

namespace Chrivent {
	class VulkanMemory {
	public:
		// 물리 디바이스 메모리 중 요청한 속성을 만족하는 memory type index를 찾는다.
		static bool FindMemoryType(const VulkanDevice& deviceInfo, uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& memoryType);
	};
}
