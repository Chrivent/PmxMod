#include "Dx12Viewer.h"

#include "Dx12Instance.h"

#include <iostream>

namespace Chrivent {
	Dx12Viewer::Dx12Viewer() {
		info = std::make_unique<Dx12ViewerInfo>();
	}

	void Dx12Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx12Viewer::Setup() {
		InitDirs("shader_hlsl");
		std::cerr << "DX12 renderer setup is not implemented yet.\n";
		return false;
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
