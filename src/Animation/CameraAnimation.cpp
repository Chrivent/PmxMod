#include "CameraAnimation.h"

#include "AnimationHelper.h"

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
	if (vmd.cameras.empty())
		return false;
	keys.clear();
	for (const auto& cam: vmd.cameras) {
		CameraAnimationKey key{};
		key.time = static_cast<int32_t>(cam.frame);
		key.interest = cam.interest * glm::vec3(1, 1, -1);
		key.rotate = cam.rotate;
		key.distance = cam.distance;
		key.fov = glm::radians(static_cast<float>(cam.viewAngle));
		key.ixBezier.Assign(
			cam.interpolation[0], cam.interpolation[1],
			cam.interpolation[2], cam.interpolation[3]);
		key.iyBezier.Assign(
			cam.interpolation[4], cam.interpolation[5],
			cam.interpolation[6], cam.interpolation[7]);
		key.izBezier.Assign(
			cam.interpolation[8], cam.interpolation[9],
			cam.interpolation[10], cam.interpolation[11]);
		key.rotateBezier.Assign(
			cam.interpolation[12], cam.interpolation[13],
			cam.interpolation[14], cam.interpolation[15]);
		key.distanceBezier.Assign(
			cam.interpolation[16], cam.interpolation[17],
			cam.interpolation[18], cam.interpolation[19]);
		key.fovBezier.Assign(
			cam.interpolation[20], cam.interpolation[21],
			cam.interpolation[22], cam.interpolation[23]);
		keys.push_back(key);
	}
	std::ranges::sort(keys, {}, &CameraAnimationKey::time);
	return true;
}

void CameraAnimation::Evaluate(const float t) {
	if (keys.empty())
		return;
	const auto it = AnimationHelper::FindUpperKey(keys, t);
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
