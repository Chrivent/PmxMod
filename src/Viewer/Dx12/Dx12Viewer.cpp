#include "Dx12Viewer.h"

#include "Dx12Instance.h"

#include <iostream>

namespace Chrivent {
	Dx12Viewer::Dx12Viewer() {
		info = std::make_unique<Dx12ViewerInfo>();
	}

	Dx12Viewer::~Dx12Viewer() {
		commandContext.WaitForGpu(device.GetInfo());
		commandContext.Destroy();
		swapChain.Destroy();
		device.Destroy();
	}

	void Dx12Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx12Viewer::Setup() {
		InitDirs("shader_hlsl");
		if (!device.Initialize()) {
			std::cerr << "Failed to initialize DX12 device.\n";
			return false;
		}
		if (!commandContext.Initialize(device.GetInfo())) {
			std::cerr << "Failed to initialize DX12 command context.\n";
			return false;
		}
		return true;
	}

	bool Dx12Viewer::Resize() {
		return true;
	}

	void Dx12Viewer::BeginFrame() {}

	bool Dx12Viewer::EndFrame() {
		return true;
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstance() const {
		return std::make_unique<Dx12Instance>();
	}

	Dx12Texture Dx12Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(texturePath);
	}
}
