#include "CameraAnimation.h"

#include "../AnimationKeySearch.h"

namespace Chrivent {
	uint32_t CameraAnimation::GetLastFrame() const {
		return info.keys.empty() ? 0 : info.keys.back().frame;
	}

	void CameraAnimation::Evaluate(const float t) {
		if (info.keys.empty())
			return;
		const auto it = AnimationKeySearch::FindUpperKey(info.keys, t);
		const auto& cur = it != info.keys.end() ? *it : info.keys.back();
		info.camera.interest = cur.interest;
		info.camera.rotate = cur.rotate;
		info.camera.distance = cur.distance;
		info.camera.fov = cur.fov;
		if (it == info.keys.begin() || it == info.keys.end())
			return;
		const auto& [frame, interest, rotate, distance, fov,
			ixBezier, iyBezier, izBezier,
			rotateBezier, distanceBezier, fovBezier] = *it;
		const auto& prev = *std::prev(it);
		if (frame - prev.frame <= 1) {
			info.camera.interest = prev.interest;
			info.camera.rotate = prev.rotate;
			info.camera.distance = prev.distance;
			info.camera.fov = prev.fov;
			return;
		}
		const float prevFrame = prev.frame;
		const float nextFrame = frame;
		const float normalizedTime = (t - prevFrame) / (nextFrame - prevFrame);
		const float ixY = ixBezier.Evaluate(normalizedTime);
		const float iyY = iyBezier.Evaluate(normalizedTime);
		const float izY = izBezier.Evaluate(normalizedTime);
		const float rY = rotateBezier.Evaluate(normalizedTime);
		const float dY = distanceBezier.Evaluate(normalizedTime);
		const float fY = fovBezier.Evaluate(normalizedTime);
		info.camera.interest = glm::mix(prev.interest, interest, glm::vec3(ixY, iyY, izY));
		info.camera.rotate = glm::mix(prev.rotate, rotate, rY);
		info.camera.distance = glm::mix(prev.distance, distance, dY);
		info.camera.fov = glm::mix(prev.fov, fov, fY);
	}
}
