#include "Core/Animation/Camera/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
	glm::mat4 Camera::CalculateProjection(const float width, const float height, const float nearPlane, const float farPlane) const {
		if (perspective)
			return glm::perspectiveFovRH(glm::max(fov, glm::radians(1.0f)), width, height, nearPlane, farPlane);
		const float extent = glm::max(std::abs(distance), 0.001f) * 0.5f;
		const float halfWidth = extent * glm::min(1.0f, width / height);
		const float halfHeight = extent * glm::min(1.0f, height / width);
		return glm::orthoRH(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
	}

	glm::mat4 Camera::CalculateViewMatrix() const {
		glm::mat4 view(1.0f);
		view = glm::translate(view, glm::vec3(0, 0, -distance));
		glm::mat4 rot(1.0f);
		rot = glm::rotate(rot, rotate.y, glm::vec3(0, 1, 0));
		rot = glm::rotate(rot, rotate.z, glm::vec3(0, 0, -1));
		rot = glm::rotate(rot, rotate.x, glm::vec3(1, 0, 0));
		view = rot * view;
		const glm::vec3 eye = glm::vec3(view[3]) + interest;
		const glm::vec3 center = glm::mat3(view) * glm::vec3(0, 0, -1) + eye;
		const glm::vec3 up = glm::mat3(view) * glm::vec3(0, 1, 0);
		return glm::lookAt(eye, center, up);
	}
}
