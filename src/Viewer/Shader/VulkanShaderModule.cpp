#include "Viewer/Shader/VulkanShaderModule.h"

namespace Chrivent {
	VulkanShaderModule::~VulkanShaderModule() {
		Reset();
	}

	GraphicsResult<void> VulkanShaderModule::Initialize(
		const VulkanDevice& sourceDevice, const std::span<const uint32_t> spvBytes) {
		device = sourceDevice.GetDevice();
		if (device == VK_NULL_HANDLE || spvBytes.empty()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "shader module 생성",
				"Vulkan device 또는 SPIR-V 셰이더 byte 크기가 올바르지 않습니다"));
		}
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = spvBytes.size() * sizeof(uint32_t);
		createInfo.pCode = spvBytes.data();
		const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
		if (result == VK_SUCCESS)
			return {};
		return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
			GraphicsErrorCode::ResourceCreationFailed, "shader module 생성",
			"Vulkan shader module을 만들지 못했습니다", result, true));
	}

	void VulkanShaderModule::Reset() {
		if (device != VK_NULL_HANDLE && shaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device, shaderModule, nullptr);
			shaderModule = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
