#include "Viewer/Drawer.h"

namespace Chrivent {
	const glm::mat4& Drawer::ClipMatrix() const {
		static constexpr glm::mat4 clipMatrix(1.0f);
		return clipMatrix;
	}

	glm::mat4 Drawer::BuildGroundShadowMatrix(const glm::vec3& lightDir) {
		constexpr glm::vec4 plane(0.0f, 1.0f, 0.0f, 0.0f);
		const glm::vec4 light(-lightDir, 0.0f);
		return glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
	}

	Drawer::~Drawer() = default;

	void Drawer::Draw() {
		DrawModel();
		DrawEdge();
		DrawGroundShadow();
	}
}
