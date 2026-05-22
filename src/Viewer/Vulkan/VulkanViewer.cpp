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
		if (!device.Initialize(GetInfo().window))
			return false;
		if (!swapChain.Initialize(device.GetInfo(), GetInfo().window))
			return false;
		if (!renderPass.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!frameBuffer.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass()))
			return false;
		if (!commandContext.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		return syncObject.Initialize(device.GetInfo());
	}

	bool VulkanViewer::Resize() {
		commandContext.Destroy();
		frameBuffer.Destroy();
		renderPass.Destroy();
		if (!swapChain.Recreate(device.GetInfo(), GetInfo().window))
			return false;
		if (!renderPass.Initialize(device.GetInfo(), swapChain.GetInfo()))
			return false;
		if (!frameBuffer.Initialize(device.GetInfo(), swapChain.GetInfo(), renderPass.GetRenderPass()))
			return false;
		return commandContext.Initialize(device.GetInfo(), swapChain.GetInfo());
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
