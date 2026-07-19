#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"

#include <span>

namespace Chrivent {
	// SPIR-V 바이트 코드를 Vulkan shader module 수명으로 감싼다.
	class VulkanShaderModule {
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanShaderModule() = default;
		~VulkanShaderModule();

		VulkanShaderModule(const VulkanShaderModule&) = delete;
		VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
		
		VkShaderModule GetShaderModule() const { return shaderModule; }

		// SPIR-V 바이트 코드에서 Vulkan shader module을 생성한다.
		GraphicsResult<void> Initialize(
			const VulkanDevice& sourceDevice, std::span<const uint32_t> spvBytes);
		// 생성한 shader module 리소스를 해제한다.
		void Reset();
	};
}
