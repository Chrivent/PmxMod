#include "CameraAnimation.h"

#include "../AnimationKeySearch.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
	glm::mat4 Camera::CalcViewMatrix() const {
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

	void CameraAnimation::Evaluate(const float t) {
		if (keys.empty())
			return;
		const auto it = AnimationKeySearch::FindUpperKey(keys, t);
		const auto& cur = it != keys.end() ? *it : keys.back();
		camera.interest = cur.interest;
		camera.rotate = cur.rotate;
		camera.distance = cur.distance;
		camera.fov = cur.fov;
		if (it == keys.begin() || it == keys.end())
			return;
		const auto& [time, interest, rotate, distance, fov,
			ixBezier, iyBezier, izBezier,
			rotateBezier, distanceBezier, fovBezier] = *it;
		const auto& prev = *(it - 1);
		if (time - prev.time <= 1) {
			camera.interest = prev.interest;
			camera.rotate = prev.rotate;
			camera.distance = prev.distance;
			camera.fov = prev.fov;
			return;
		}
		const float normalizedTime = (t - static_cast<float>(prev.time)) / static_cast<float>(time - prev.time);
		const float ixY = ixBezier.Evaluate(normalizedTime);
		const float iyY = iyBezier.Evaluate(normalizedTime);
		const float izY = izBezier.Evaluate(normalizedTime);
		const float rY = rotateBezier.Evaluate(normalizedTime);
		const float dY = distanceBezier.Evaluate(normalizedTime);
		const float fY = fovBezier.Evaluate(normalizedTime);
		camera.interest = glm::mix(prev.interest, interest, glm::vec3(ixY, iyY, izY));
		camera.rotate = glm::mix(prev.rotate, rotate, rY);
		camera.distance = glm::mix(prev.distance, distance, dY);
		camera.fov = glm::mix(prev.fov, fov, fY);
	}
}
