#include "CameraAnimation.h"

#include <glm/gtc/matrix_transform.hpp>

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

bool CameraAnimation::Create(const VmdReader& vmd) {
	if (!vmd.cameras.empty()) {
		keys.clear();
		for (const auto& cam: vmd.cameras) {
			CameraAnimationKey key{};
			key.time = static_cast<int32_t>(cam.frame);
			key.interest = cam.interest * glm::vec3(1, 1, -1);
			key.rotate = cam.rotate;
			key.distance = cam.distance;
			key.fov = glm::radians(static_cast<float>(cam.viewAngle));
			AssignBezier(key.ixBezier,
				cam.interpolation[0], cam.interpolation[1],
				cam.interpolation[2], cam.interpolation[3]);
			AssignBezier(key.iyBezier,
				cam.interpolation[4], cam.interpolation[5],
				cam.interpolation[6], cam.interpolation[7]);
			AssignBezier(key.izBezier,
				cam.interpolation[8], cam.interpolation[9],
				cam.interpolation[10], cam.interpolation[11]);
			AssignBezier(key.rotateBezier,
				cam.interpolation[12], cam.interpolation[13],
				cam.interpolation[14], cam.interpolation[15]);
			AssignBezier(key.distanceBezier,
				cam.interpolation[16], cam.interpolation[17],
				cam.interpolation[18], cam.interpolation[19]);
			AssignBezier(key.fovBezier,
				cam.interpolation[20], cam.interpolation[21],
				cam.interpolation[22], cam.interpolation[23]);
			keys.push_back(key);
		}
		std::ranges::sort(keys, {}, &CameraAnimationKey::time);
	} else
		return false;
	return true;
}

void CameraAnimation::Evaluate(const float t) {
	if (keys.empty())
		return;
	const auto it = std::ranges::upper_bound(keys, t, std::less{},
		[](const CameraAnimationKey& k) { return static_cast<float>(k.time); });
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
	const float ixY = Bezier(FindBezierX(normalizedTime, ixBezier.first.x, ixBezier.second.x), ixBezier.first.y, ixBezier.second.y);
	const float iyY = Bezier(FindBezierX(normalizedTime, iyBezier.first.x, iyBezier.second.x), iyBezier.first.y, iyBezier.second.y);
	const float izY = Bezier(FindBezierX(normalizedTime, izBezier.first.x, izBezier.second.x), izBezier.first.y, izBezier.second.y);
	const float rY = Bezier(FindBezierX(normalizedTime, rotateBezier.first.x, rotateBezier.second.x), rotateBezier.first.y, rotateBezier.second.y);
	const float dY = Bezier(FindBezierX(normalizedTime, distanceBezier.first.x, distanceBezier.second.x), distanceBezier.first.y, distanceBezier.second.y);
	const float fY = Bezier(FindBezierX(normalizedTime, fovBezier.first.x, fovBezier.second.x), fovBezier.first.y, fovBezier.second.y);
	camera.interest = glm::mix(prev.interest, interest, glm::vec3(ixY, iyY, izY));
	camera.rotate = glm::mix(prev.rotate, rotate, rY);
	camera.distance = glm::mix(prev.distance, distance, dY);
	camera.fov = glm::mix(prev.fov, fov, fY);
}
