#include "Animation.h"

#include "Model.h"
#include "Util.h"

#include <map>
#include <ranges>

void Bezier::Assign(const int x0, const int x1, const int y0, const int y1) {
	auto Normalize = [](const int value) { return static_cast<float>(value) / 127.0f; };
	p1 = glm::vec2(Normalize(x0), Normalize(y0));
	p2 = glm::vec2(Normalize(x1), Normalize(y1));
}

float Bezier::Evaluate(const float time) const {
	const float bezierParameter = FindBezierX(time, p1.x, p2.x);
	return EvaluateBezier(bezierParameter, p1.y, p2.y);
}

float Bezier::EvaluateBezier(const float t, const float p1, const float p2) {
	const float it = 1.0f - t;
	return 3.0f * it * it * t * p1 + 3.0f * it * t * t * p2 + t * t * t;
}

float Bezier::FindBezierX(float time, const float x1, const float x2) {
	time = std::clamp(time, 0.0f, 1.0f);
	float start = 0.0f, stop = 1.0f;
	float t = 0.5f;
	while (true) {
		const float x = EvaluateBezier(t, x1, x2);
		const float diff = time - x;
		if (std::abs(diff) < 1e-5f)
			break;
		(diff < 0.0f ? stop : start) = t;
		t = (start + stop) * 0.5f;
	}
	return t;
}

void NodeAnimationKey::ApplyMotion(const VmdReader::VmdMotion& motion) {
	time = static_cast<int32_t>(motion.frame);
	translate = motion.translate * glm::vec3(1, 1, -1);
	const glm::quat q = motion.quaternion;
	const auto rot = Util::InvZ(glm::mat3_cast(q));
	rotate = glm::quat_cast(rot);
	txBezier.Assign(
		motion.interpolation[0], motion.interpolation[8],
		motion.interpolation[4], motion.interpolation[12]);
	tyBezier.Assign(
		motion.interpolation[1], motion.interpolation[9],
		motion.interpolation[5], motion.interpolation[13]);
	tzBezier.Assign(
		motion.interpolation[2], motion.interpolation[10],
		motion.interpolation[6], motion.interpolation[14]);
	rotBezier.Assign(
		motion.interpolation[3], motion.interpolation[11],
		motion.interpolation[7], motion.interpolation[15]);
}

void Animation::AddNodeAnimations(const VmdReader& vmd) {
	std::map<std::string, NodeAnimationTrack> nodeMap;
	for (auto& track : nodeTracks) {
		if (track.node)
			nodeMap.emplace(track.node->name, std::move(track));
	}
	nodeTracks.clear();
	for (const auto& motion : vmd.motions) {
		auto nodeName = Util::SjisToUtf8(motion.boneName);
		auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
		auto& [node, keys] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(model->nodes, nodeName,
				[](const std::shared_ptr<Node>& candidate) { return candidate->name; });
			node = it != model->nodes.end() ? *it : nullptr;
		}
		if (!node)
			continue;
		keys.emplace_back().ApplyMotion(motion);
	}
	for (auto& track : nodeMap | std::views::values) {
		std::ranges::sort(track.keys, {}, &NodeAnimationKey::time);
		nodeTracks.emplace_back(std::move(track));
	}
}

void Animation::AddIkAnimations(const VmdReader& vmd) {
	std::map<std::string, IkAnimationTrack> ikMap;
	for (auto& track : ikTracks) {
		if (track.ikSolver) {
			if (const auto ikNodePtr = track.ikSolver->ikNode.lock())
				ikMap.emplace(ikNodePtr->name, std::move(track));
		}
	}
	ikTracks.clear();
	for (const auto& ik : vmd.iks) {
		for (const auto& [name, enable] : ik.ikInfos) {
			auto ikName = Util::SjisToUtf8(name);
			auto [findIt, inserted] = ikMap.try_emplace(ikName);
			auto& [ikSolver, keys] = findIt->second;
			if (inserted) {
				auto it = std::ranges::find(model->ikSolvers, ikName,
					[](const std::shared_ptr<IkSolver>& candidate){
						const auto ikNodePtr = candidate->ikNode.lock();
						return ikNodePtr ? ikNodePtr->name : std::string{};
				});
				ikSolver = it != model->ikSolvers.end() ? *it : nullptr;
			}
			if (!ikSolver)
				continue;
			auto& [time, ikEnable] = keys.emplace_back();
			time = static_cast<int32_t>(ik.frame);
			ikEnable = enable != 0;
		}
	}
	for (auto& track : ikMap | std::views::values) {
		std::ranges::sort(track.keys, {}, &IkAnimationKey::time);
		ikTracks.emplace_back(std::move(track));
	}
}

void Animation::AddMorphAnimations(const VmdReader& vmd) {
	std::map<std::string, MorphAnimationTrack> morphMap;
	for (auto& track : morphTracks) {
		if (track.morph)
			morphMap.emplace(track.morph->name, std::move(track));
	}
	morphTracks.clear();
	for (const auto& [blendShapeName, frame, weight] : vmd.morphs) {
		auto morphName = Util::SjisToUtf8(blendShapeName);
		auto [findIt, inserted] = morphMap.try_emplace(morphName);
		auto& [morph, keys] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(model->morphs, morphName, &Morph::name);
			morph = it != model->morphs.end() ? std::shared_ptr<Morph>(model, it->get()) : nullptr;
		}
		if (!morph)
			continue;
		auto& [time, morphWeight] = keys.emplace_back();
		time = static_cast<int32_t>(frame);
		morphWeight = weight;
	}
	for (auto& track : morphMap | std::views::values) {
		std::ranges::sort(track.keys, {}, &MorphAnimationKey::time);
		morphTracks.emplace_back(std::move(track));
	}
}

void Animation::EvaluateNodes(const float t, const float animWeight) const {
	for (const auto& [node, keys]: nodeTracks) {
		if (!node)
			continue;
		if (keys.empty()) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const NodeAnimationKey& k) { return static_cast<float>(k.time); });
		const auto& cur = it != keys.end() ? *it : keys.back();
		glm::vec3 vt = cur.translate;
		glm::quat q  = cur.rotate;
		if (it != keys.begin() && it != keys.end()) {
			const auto& prev = *(it - 1);
			const auto& [time, translate, rotate,
				txBezier, tyBezier, tzBezier,
				rotBezier] = *it;
			const auto timeRange = static_cast<float>(time - prev.time);
			const float normalizedTime = (t - static_cast<float>(prev.time)) / timeRange;
			const float txY = txBezier.Evaluate(normalizedTime);
			const float tyY = tyBezier.Evaluate(normalizedTime);
			const float tzY = tzBezier.Evaluate(normalizedTime);
			const float rotY = rotBezier.Evaluate(normalizedTime);
			vt = glm::mix(prev.translate, translate, glm::vec3(txY, tyY, tzY));
			q  = glm::slerp(prev.rotate,   rotate,   rotY);
		}
		node->animTranslate = animWeight != 1.0f ? glm::mix(node->baseAnimTranslate, vt, animWeight) : vt;
		node->animRotate = animWeight != 1.0f ? glm::slerp(node->baseAnimRotate, q, animWeight) : q;
	}
}

void Animation::EvaluateIks(const float t, const float animWeight) const {
	for (const auto& [ikSolver, keys] : ikTracks) {
		if (!ikSolver)
			continue;
		if (keys.empty()) {
			ikSolver->enable = true;
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
		[](const IkAnimationKey& k) { return static_cast<float>(k.time); });
		const bool enable = it != keys.begin() ? (it - 1)->ikEnable : keys.begin()->ikEnable;
		ikSolver->enable = animWeight < 1.0f ? ikSolver->baseAnimEnable : enable;
	}
}

void Animation::EvaluateMorphs(const float t, const float animWeight) const {
	for (const auto& [morph, keys] : morphTracks) {
		if (!morph)
			continue;
		if (keys.empty())
			continue;
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
		[](const MorphAnimationKey& k) { return static_cast<float>(k.time); });
		float weight = it != keys.end() ? it->morphWeight : keys.back().morphWeight;
		if (it != keys.begin() && it != keys.end()) {
			auto [time0, weight0] = *(it - 1);
			auto [time1, weight1] = *it;
			const float time = (t - static_cast<float>(time0)) / static_cast<float>(time1 - time0);
			weight = (weight1 - weight0) * time + weight0;
		}
		morph->weight = animWeight != 1.0f ? glm::mix(morph->saveAnimWeight, weight, animWeight) : weight;
	}
}

bool Animation::Add(const VmdReader& vmd) {
	AddNodeAnimations(vmd);
	AddIkAnimations(vmd);
	AddMorphAnimations(vmd);
	return true;
}

void Animation::Destroy() {
	model.reset();
	nodeTracks.clear();
	ikTracks.clear();
	morphTracks.clear();
}

void Animation::Evaluate(const float t, const float animWeight) const {
	EvaluateNodes(t, animWeight);
	EvaluateIks(t, animWeight);
	EvaluateMorphs(t, animWeight);
}

void Animation::SyncPhysics(const float t) const {
	model->SaveBaseAnimation();
	for (int i = 0; i < 30; i++) {
		model->BeginAnimation();
		Evaluate(t, static_cast<float>(1 + i) / 30.0f);
		model->UpdateMorphAnimation();
		model->UpdateNodeAnimation(false);
		model->UpdatePhysicsAnimation(1.0f / 30.0f);
		model->UpdateNodeAnimation(true);
	}
}

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
