#include "Viewer/Error/OpenGlError.h"

namespace Chrivent {
	void OpenGlError::Clear() {
		while (glGetError() != GL_NO_ERROR) {}
	}

	GLenum OpenGlError::Take() {
		const GLenum firstError = glGetError();
		while (glGetError() != GL_NO_ERROR) {}
		return firstError;
	}
}
