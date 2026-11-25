#include "Core/Animation/Camera/CameraAnimation.h"

#include "Core/Animation/AnimationKeySequence.h"

#include <utility>

namespace Chrivent {
	CameraAnimation::CameraAnimation(std::vector<CameraAnimationKey> animationKeys, std::vector<LightAnimationKey> lightKeys)
		: keys(std::move(animationKeys)), lights(std::move(lightKeys)) {
		AnimationKeySequence::SortAndKeepLastKeyPerFrame(keys);
		AnimationKeySequence::SortAndKeepLastKeyPerFrame(lights);
	}

	uint32_t CameraAnimation::GetLastFrame() const {
		return std::max(keys.empty() ? 0u : keys.back().frame, lights.empty() ? 0u : lights.back().frame);
	}

	SceneLighting CameraAnimation::EvaluateLighting(const float t) const {
		SceneLighting state;
		if (lights.empty())
			return state;
		const auto next = AnimationKeySequence::FindUpperKey(lights, t);
		const auto& from = next == lights.begin() ? lights.front() : *std::prev(next);
		const auto& [frame, color, direction] = next == lights.end() ? from : *next;
		const float weight = frame == from.frame ? 0.0f : (t - from.frame) / (frame - from.frame);
		state.color = glm::mix(from.color, color, weight);
		const glm::vec3 interpolatedDirection = glm::mix(from.direction, direction, weight);
		if (glm::dot(interpolatedDirection, interpolatedDirection) > 1e-10f)
			state.direction = interpolatedDirection;
		return state;
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
		camera.perspective = cur.perspective;
		if (it == keys.begin() || it == keys.end())
			return camera;
		const auto& [frame, interest, rotate, distance, fov,
			ixBezier, iyBezier, izBezier,
			rotateBezier, distanceBezier, fovBezier, perspective] = *it;
		const auto& prev = *std::prev(it);
		camera.perspective = prev.perspective;
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
