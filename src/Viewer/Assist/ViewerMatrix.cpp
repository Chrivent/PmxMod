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

	glm::mat4 ViewerMatrix::BuildGroundShadowMatrix(const glm::vec3& lightDir) {
		constexpr glm::vec4 plane(0.0f, 1.0f, 0.0f, 0.0f);
		const glm::vec4 light(-lightDir, 0.0f);
		return glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
	}
}
