#pragma once

#include "VulkanDevice.h"

#include <vector>

namespace Chrivent {
	class VulkanShaderModule {
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanShaderModule() = default;
		~VulkanShaderModule();

		VulkanShaderModule(const VulkanShaderModule&) = delete;
		VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
		VulkanShaderModule(VulkanShaderModule&&) = delete;
		VulkanShaderModule& operator=(VulkanShaderModule&&) = delete;
		
		VkShaderModule GetShaderModule() const { return shaderModule; }

		// SPIR-V 바이트 코드에서 Vulkan shader module을 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const std::vector<char>& spvBytes);
		// 생성한 shader module 리소스를 해제한다.
		void Destroy();
	};
}
