#include "VulkanViewer.h"

#include "VulkanInstance.h"

namespace Chrivent {
	VulkanViewer::VulkanViewer() {
		info = std::make_unique<VulkanViewerInfo>();
	}

	void VulkanViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool VulkanViewer::Setup() {
		InitDirs("shader_Vulkan");
		return device.Initialize(GetInfo().window);
	}

	bool VulkanViewer::Resize() {
		return true;
	}

	void VulkanViewer::BeginFrame() {
	}

	bool VulkanViewer::EndFrame() {
		return true;
	}

	std::unique_ptr<Instance> VulkanViewer::CreateInstance() const {
		return std::make_unique<VulkanInstance>();
	}

	VulkanTexture VulkanViewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(texturePath);
	}
}
