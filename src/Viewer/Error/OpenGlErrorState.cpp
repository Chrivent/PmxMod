#include "Viewer/Error/OpenGlErrorState.h"

namespace Chrivent {
	void OpenGlErrorState::Clear() {
		while (glGetError() != GL_NO_ERROR) {}
	}

	GLenum OpenGlErrorState::Take() {
		const GLenum firstError = glGetError();
		while (glGetError() != GL_NO_ERROR) {}
		return firstError;
	}
}
