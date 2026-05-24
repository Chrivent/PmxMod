#include "VulkanShaderModule.h"

#include <fstream>
#include <iostream>

namespace Chrivent {
	VulkanShaderModule::~VulkanShaderModule() {
		Destroy();
	}

	bool VulkanShaderModule::Initialize(const VulkanDeviceInfo& deviceInfo, const std::filesystem::path& file) {
		device = deviceInfo.device;
		std::vector<char> code;
		if (!ReadSpvFile(file, code))
			return false;
		if (code.empty() || code.size() % sizeof(uint32_t) != 0) {
			std::cerr << "Invalid SPIR-V shader file size: " << file.string() << '\n';
			return false;
		}
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan shader module: " << file.string() << '\n';
			return false;
		}
		return true;
	}

	void VulkanShaderModule::Destroy() {
		if (device != VK_NULL_HANDLE && shaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device, shaderModule, nullptr);
			shaderModule = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}

	bool VulkanShaderModule::ReadSpvFile(const std::filesystem::path& file, std::vector<char>& code) {
		std::ifstream stream(file, std::ios::ate | std::ios::binary);
		if (!stream) {
			std::cerr << "Failed to open SPIR-V shader file: " << file.string() << '\n';
			return false;
		}
		const std::streamsize fileSize = stream.tellg();
		if (fileSize <= 0) {
			std::cerr << "SPIR-V shader file is empty: " << file.string() << '\n';
			return false;
		}
		code.resize(static_cast<size_t>(fileSize));
		stream.seekg(0);
		if (!stream.read(code.data(), fileSize)) {
			std::cerr << "Failed to read SPIR-V shader file: " << file.string() << '\n';
			return false;
		}
		return true;
	}
}
