#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	// 시간 기반 후처리 패스가 공유하는 현재 프레임과 카메라 입력을 보관한다.
	struct PostProcessFrameData {
		float deltaTime = 0.0f;
		float nearPlane = 1.0f;
		float farPlane = 10000.0f;
		float verticalFovRadians = 0.5235988f;
		float viewportWidth = 0.0f;
		float viewportHeight = 0.0f;
		float inverseViewportWidth = 0.0f;
		float inverseViewportHeight = 0.0f;
		float historyReset = 1.0f;
		float padding0 = 0.0f;
		float padding1 = 0.0f;
		float padding2 = 0.0f;
		glm::vec4 cameraWorldPosition{};
		glm::vec4 previousCameraWorldPosition{};
		glm::vec4 cameraWorldDirection{};
		glm::vec4 previousCameraWorldDirection{};
		glm::vec4 cameraWorldRight{};
		glm::vec4 cameraWorldUp{};
	};
}
