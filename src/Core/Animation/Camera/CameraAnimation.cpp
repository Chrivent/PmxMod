#include "Core/Animation/Camera/CameraAnimation.h"

#include "Core/Animation/AnimationKeySequence.h"

#include <utility>

namespace Chrivent {
	CameraAnimation::CameraAnimation(std::vector<CameraAnimationKey> animationKeys)
		: keys(std::move(animationKeys)) {
		AnimationKeySequence::SortAndKeepLastKeyPerFrame(keys);
	}

	Camera CameraAnimation::Evaluate(const float t) const {
		Camera camera;
		if (keys.empty())
			return camera;
		const auto it = AnimationKeySequence::FindUpperKey(keys, t);
		const auto& cur = it != keys.end() ? *it : keys.back();
		camera.interest = cur.interest;
		camera.rotate = cur.rotate;
		camera.distance = cur.distance;
		camera.fov = cur.fov;
		if (it == keys.begin() || it == keys.end())
			return camera;
		const auto& [frame, interest, rotate, distance, fov,
			ixBezier, iyBezier, izBezier,
			rotateBezier, distanceBezier, fovBezier] = *it;
		const auto& prev = *std::prev(it);
		if (frame - prev.frame <= 1) {
			camera.interest = prev.interest;
			camera.rotate = prev.rotate;
			camera.distance = prev.distance;
			camera.fov = prev.fov;
			return camera;
		}
		const float prevFrame = static_cast<float>(prev.frame);
		const float nextFrame = static_cast<float>(frame);
		const float normalizedTime = (t - prevFrame) / (nextFrame - prevFrame);
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
		return camera;
	}
}
