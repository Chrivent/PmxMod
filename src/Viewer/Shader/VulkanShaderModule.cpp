#include "Viewer/Shader/VulkanShaderModule.h"

namespace Chrivent {
	VulkanShaderModule::~VulkanShaderModule() {
		Reset();
	}

	bool VulkanShaderModule::Initialize(const VulkanDevice& sourceDevice,
		const std::span<const uint32_t> spvBytes, std::string& error) {
		error.clear();
		device = sourceDevice.GetDevice();
		if (spvBytes.empty()) {
			error = "SPIR-V 셰이더 byte 크기가 올바르지 않습니다";
			return false;
		}
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = spvBytes.size() * sizeof(uint32_t);
		createInfo.pCode = spvBytes.data();
		const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
		if (result != VK_SUCCESS) {
			error = "Vulkan shader module을 만들지 못했습니다 (네이티브 코드: "
				+ std::to_string(result) + ')';
			return false;
		}
		return true;
	}

	void VulkanShaderModule::Reset() {
		if (device != VK_NULL_HANDLE && shaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device, shaderModule, nullptr);
			shaderModule = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
