#include "ViewerMatrix.h"

namespace Chrivent {
	const glm::mat4& ViewerMatrix::DirectXClipMatrix() {
		static constexpr glm::mat4 dxClipMatrix(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return dxClipMatrix;
	}

	const glm::mat4& ViewerMatrix::VulkanClipMatrix() {
		static constexpr glm::mat4 vulkanClipMatrix(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f
		);
		return vulkanClipMatrix;
	}

}
