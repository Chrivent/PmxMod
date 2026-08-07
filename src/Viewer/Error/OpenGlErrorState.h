#pragma once

#include "Viewer/Error/GraphicsError.h"

#include <glad/glad.h>

namespace Chrivent {
	// OpenGL의 누적 오류 상태를 작업 경계별로 비우고 수집한다.
	class OpenGlErrorState {
	public:
		// 이전 OpenGL 작업에서 남은 오류를 모두 비운다.
		static void Clear();
		// 현재까지 발생한 OpenGL 오류를 모두 소비하고 첫 오류를 반환한다.
		static GLenum Take();
		// 현재 OpenGL 오류를 소비하고 구조화된 작업 결과로 변환한다.
		static GraphicsError::Result<void> ResolveResult(GraphicsErrorCode code,
			std::string operation, std::string message);
	};
}
