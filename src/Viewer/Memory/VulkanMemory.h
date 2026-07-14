#pragma once

#include "Viewer/Device/VulkanDevice.h"

namespace Chrivent {
	// Vulkan 리소스 요구 조건에 맞는 메모리 형식을 선택한다.
	class VulkanMemory {
	public:
		// 물리 디바이스 메모리 중 요청한 속성을 만족하는 memory type index를 찾는다.
		static bool FindMemoryType(const VulkanDevice& sourceDevice, uint32_t typeFilter,
			VkMemoryPropertyFlags properties, uint32_t& memoryType);
	};
}
