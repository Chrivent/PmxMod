#pragma once

#include "VulkanDevice.h"

#include <filesystem>

namespace Chrivent {
	class VulkanShaderModule {
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;

		// SPIR-V 셰이더 파일을 바이너리 데이터로 읽는다.
		static bool ReadSpvFile(const std::filesystem::path& file, std::vector<char>& code);

	public:
		VulkanShaderModule() = default;
		~VulkanShaderModule();

		VulkanShaderModule(const VulkanShaderModule&) = delete;
		VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
		VulkanShaderModule(VulkanShaderModule&&) = delete;
		VulkanShaderModule& operator=(VulkanShaderModule&&) = delete;
		
		VkShaderModule GetShaderModule() const { return shaderModule; }

		// SPIR-V 바이너리 파일에서 Vulkan shader module을 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const std::filesystem::path& file);
		// 생성한 shader module 리소스를 해제한다.
		void Destroy();
	};
}
