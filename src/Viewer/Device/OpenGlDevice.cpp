#include "Viewer/Device/OpenGlDevice.h"

#include <algorithm>
#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Chrivent {
	void* OpenGlDevice::LoadGlProc(const char* name) {
		return reinterpret_cast<void*>(glfwGetProcAddress(name));
	}

	const char* OpenGlDevice::ResolveGpuTypeName(const std::string& renderer) {
		if (renderer.contains("NVIDIA") || renderer.contains("Radeon RX")
			|| renderer.contains("Radeon Pro"))
			return "discrete";
		if (renderer.contains("Intel") || renderer.contains("Radeon(TM) Graphics"))
			return "integrated";
		return "other";
	}

	GraphicsResult<void> OpenGlDevice::Initialize(GLFWwindow* window,
		const uint32_t preferredSampleCount, GraphicsCapabilities& capabilities) {
		capabilities = {};
		if (window == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::InvalidArgument, "OpenGL device 초기화",
				"OpenGL 컨텍스트를 만들 GLFW 윈도우가 없습니다"));
		}
		glfwMakeContextCurrent(window);
		if (!gladLoadGLLoader(LoadGlProc)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::InitializationFailed, "OpenGL 함수 로드",
				"GLAD가 필요한 함수를 찾지 못했습니다"));
		}
		GLint majorVersion = 0;
		GLint minorVersion = 0;
		GLint maxSamples = 1;
		GLint uniformAlignment = 1;
		GLint maxTextureBindings = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniformAlignment);
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureBindings);
		if (majorVersion < 4 || (majorVersion == 4 && minorVersion < 6)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::UnsupportedFeature, "OpenGL 버전 확인",
				"OpenGL 4.6 이상이 필요합니다"));
		}
		const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		const auto* shaderVersion = reinterpret_cast<const char*>(
			glGetString(GL_SHADING_LANGUAGE_VERSION));
		capabilities.apiName = "OpenGL";
		capabilities.apiVersion = version ? version : "4.6";
		capabilities.shaderVersion = shaderVersion ? shaderVersion : "GLSL 4.60";
		capabilities.gpuName = renderer ? renderer : "unknown";
		capabilities.gpuType = ResolveGpuTypeName(capabilities.gpuName);
		capabilities.maxSampleCount = static_cast<uint32_t>(std::max(maxSamples, 1));
		capabilities.activeSampleCount = std::min(preferredSampleCount, capabilities.maxSampleCount);
		capabilities.uniformBufferAlignment = static_cast<uint64_t>(std::max(uniformAlignment, 1));
		capabilities.maxTextureBindings = static_cast<uint32_t>(std::max(maxTextureBindings, 0));
		glfwSwapInterval(0);
		glEnable(GL_MULTISAMPLE);
		return {};
	}

	GraphicsResult<void> OpenGlDevice::WaitIdle(GLFWwindow* window) {
		if (window == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::InvalidState, "GPU 대기",
				"OpenGL 윈도우를 사용할 수 없습니다"));
		}
		glfwMakeContextCurrent(window);
		glFinish();
		return {};
	}
}
