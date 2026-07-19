#pragma once

#include "Viewer/Device/GraphicsCapabilities.h"
#include "Viewer/Error/GraphicsError.h"

#include <cstdint>

// ReSharper disable once CppInconsistentNaming
struct GLFWwindow;

namespace Chrivent {
	// OpenGL 함수 로드, 버전 검증과 그래픽 기능 조회를 담당한다.
	class OpenGlDevice {
		// GLAD가 사용할 OpenGL 함수 포인터를 GLFW에서 조회한다.
		static void* LoadGlProc(const char* name);
		// OpenGL renderer 이름을 GPU 종류 이름으로 분류한다.
		static const char* ResolveGpuTypeName(const std::string& renderer);

	public:
		// 현재 윈도우의 OpenGL 컨텍스트를 초기화하고 지원 기능을 기록한다.
		static GraphicsError::Result<void> Initialize(GLFWwindow* window, uint32_t preferredSampleCount,
			GraphicsCapabilities& capabilities);
		// 현재 OpenGL 컨텍스트의 명령이 모두 처리될 때까지 기다린다.
		static GraphicsError::Result<void> WaitIdle(GLFWwindow* window);
	};
}
