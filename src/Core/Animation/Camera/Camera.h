#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	// 카메라와 독립적으로 재생되는 장면의 조명색과 방향이다.
	struct SceneLighting {
		glm::vec3 color{154.0f / 255.0f};
		// MMD의 기본 방향 (-0.5, -1, +0.5)을 모델과 같은 오른손 좌표계로 변환한 값이다.
		glm::vec3 direction{-0.5f, -1.0f, -0.5f};
	};

	// 카메라의 시선 위치, 회전, 거리와 시야각 상태를 보관한다.
	struct Camera {
		glm::vec3 interest = glm::vec3(0, 10, 0);
		glm::vec3 rotate = glm::vec3(0, 0, 0);
		float distance = 50;
		float fov = glm::radians(30.0f);
		bool perspective = true;

		// 현재 카메라 파라미터로 뷰 행렬을 계산한다.
		glm::mat4 CalculateViewMatrix() const;
		glm::mat4 CalculateProjection(float width, float height, float nearPlane, float farPlane) const;
	};
}
