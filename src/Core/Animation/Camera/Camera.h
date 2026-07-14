#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	// 카메라의 시선 위치, 회전, 거리와 시야각 상태를 보관한다.
	struct Camera {
		glm::vec3 interest = glm::vec3(0, 10, 0);
		glm::vec3 rotate = glm::vec3(0, 0, 0);
		float distance = 50;
		float fov = glm::radians(30.0f);

		// 현재 카메라 파라미터로 뷰 행렬을 계산한다.
		glm::mat4 CalcViewMatrix() const;
	};
}
