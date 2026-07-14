#include "Viewer/Shader/VulkanShaderModule.h"

#include <iostream>

namespace Chrivent {
	VulkanShaderModule::~VulkanShaderModule() {
		Reset();
	}

	bool VulkanShaderModule::Initialize(const VulkanDevice& sourceDevice, const std::span<const uint32_t> spvBytes) {
		device = sourceDevice.device;
		if (spvBytes.empty()) {
			std::cerr << "Invalid SPIR-V shader byte size.\n";
			return false;
		}
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = spvBytes.size() * sizeof(uint32_t);
		createInfo.pCode = spvBytes.data();
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan shader module.\n";
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
