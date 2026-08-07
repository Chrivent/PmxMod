#include "Viewer/Error/OpenGlErrorState.h"

#include <utility>

namespace Chrivent {
	void OpenGlErrorState::Clear() {
		while (glGetError() != GL_NO_ERROR) {}
	}

	GLenum OpenGlErrorState::Take() {
		const GLenum firstError = glGetError();
		while (glGetError() != GL_NO_ERROR) {}
		return firstError;
	}

	GraphicsError::Result<void> OpenGlErrorState::ResolveResult(const GraphicsErrorCode code,
		std::string operation, std::string message) {
		const GLenum result = Take();
		if (result == GL_NO_ERROR)
			return {};
		return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl, code,
			std::move(operation), std::move(message), result, true));
	}
}
