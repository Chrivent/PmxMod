#pragma once

#include "Core/Animation/Bezier.h"
#include "Core/Animation/Camera/Camera.h"

#include <cstdint>
#include <vector>

namespace Chrivent {
	// 한 프레임의 카메라 값과 보간 곡선을 보관한다.
	struct CameraAnimationKey {
		uint32_t	frame = 0;
		glm::vec3	interest = glm::vec3(0, 10, 0);
		glm::vec3	rotate = glm::vec3(0);
		float		distance = 50;
		float		fov = glm::radians(30.0f);
		Bezier		ixBezier;
		Bezier		iyBezier;
		Bezier		izBezier;
		Bezier		rotateBezier;
		Bezier		distanceBezier;
		Bezier		fovBezier;
	};

	// 카메라 키 목록을 평가해 지정한 시간의 카메라 상태를 만든다.
	class CameraAnimation {
		std::vector<CameraAnimationKey> keys;

	public:
		explicit CameraAnimation(std::vector<CameraAnimationKey> animationKeys);

		const std::vector<CameraAnimationKey>& GetKeys() const { return keys; }
		uint32_t GetLastFrame() const { return keys.empty() ? 0 : keys.back().frame; }

		// 지정한 시간의 카메라 키를 보간해 현재 카메라를 반환한다.
		Camera Evaluate(float t) const;
	};
}
