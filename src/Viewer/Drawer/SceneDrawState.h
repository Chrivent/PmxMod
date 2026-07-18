#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	// 카메라, 조명과 내장 패스 활성 상태를 한 프레임의 장면 입력으로 묶는다.
	struct SceneRenderState {
		glm::mat4 viewMatrix{1.0f};
		glm::mat4 projectionMatrix{1.0f};
		glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
		glm::vec3 lightDirection{-0.5f, -1.0f, 0.5f};
		bool modelEnabled = true;
		bool edgeEnabled = true;
		bool groundShadowEnabled = true;
	};

	// Drawer가 한 프레임 동안 읽을 장면, 화면과 temporal 상태의 불변 스냅샷을 나타낸다.
	struct SceneDrawState {
		SceneRenderState scene;
		glm::mat4 previousViewMatrix{1.0f};
		glm::mat4 previousProjectionMatrix{1.0f};
		glm::vec2 screenSize{};
		bool historyReset = true;
		bool velocityRequired = false;
	};
}
