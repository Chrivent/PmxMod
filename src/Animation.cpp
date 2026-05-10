#include "Animation.h"

#include "Model.h"
#include "Util.h"

#include <ranges>

void AssignBezier(std::pair<glm::vec2, glm::vec2>& bezier, const int x0, const int x1, const int y0, const int y1) {
	bezier.first = glm::vec2(static_cast<float>(x0) / 127.0f, static_cast<float>(y0) / 127.0f);
	bezier.second = glm::vec2(static_cast<float>(x1) / 127.0f, static_cast<float>(y1) / 127.0f);
}

float Bezier(const float t, const float p1, const float p2) {
	const float it = 1.0f - t;
	return 3.0f * it * it * t * p1 + 3.0f * it * t * t * p2 + t * t * t;
}

float FindBezierX(float time, const float x1, const float x2) {
	time = std::clamp(time, 0.0f, 1.0f);
	float start = 0.0f, stop = 1.0f;
	float t = 0.5f;
	while (true) {
		const float x = Bezier(t, x1, x2);
		const float diff = time - x;
		if (std::abs(diff) < 1e-5f)
			break;
		(diff < 0.0f ? stop : start) = t;
		t = (start + stop) * 0.5f;
	}
	return t;
}

void NodeAnimationKey::ApplyMotion(const VMDReader::VMDMotion& motion) {
	time = static_cast<int32_t>(motion.m_frame);
	translate = motion.m_translate * glm::vec3(1, 1, -1);
	const glm::quat q = motion.m_quaternion;
	const auto rot = Util::InvZ(glm::mat3_cast(q));
	rotate = glm::quat_cast(rot);
	AssignBezier(txBezier,
		motion.m_interpolation[0], motion.m_interpolation[8],
		motion.m_interpolation[4], motion.m_interpolation[12]);
	AssignBezier(tyBezier,
		motion.m_interpolation[1], motion.m_interpolation[9],
		motion.m_interpolation[5], motion.m_interpolation[13]);
	AssignBezier(tzBezier,
		motion.m_interpolation[2], motion.m_interpolation[10],
		motion.m_interpolation[6], motion.m_interpolation[14]);
	AssignBezier(rotBezier,
		motion.m_interpolation[3], motion.m_interpolation[11],
		motion.m_interpolation[7], motion.m_interpolation[15]);
}

bool Animation::Add(const VMDReader& vmd) {
	std::map<std::string, std::pair<std::shared_ptr<Node>, std::vector<NodeAnimationKey>>> nodeMap;
	for (auto& node : m_nodes) {
		if (node.first)
			nodeMap.emplace(node.first->m_name, std::move(node));
	}
	m_nodes.clear();
	for (const auto& motion : vmd.m_motions) {
		auto nodeName = Util::SjisToUtf8(motion.m_boneName);
		auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
		auto& [first, second] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(model->nodes, nodeName,
				[](const std::shared_ptr<Node>& node) { return node->m_name; });
			first = it != model->nodes.end() ? *it : nullptr;
		}
		if (!first)
			continue;
		second.emplace_back().ApplyMotion(motion);
	}
	for (auto& val : nodeMap | std::views::values) {
		std::ranges::sort(val.second, {}, &NodeAnimationKey::time);
		m_nodes.insert(std::move(val));
	}
	std::map<std::string, std::pair<std::shared_ptr<IkSolver>, std::vector<IKAnimationKey>>> ikMap;
	for (auto& ik : m_iks) {
		if (ik.first) {
			if (const auto ikNodePtr = ik.first->ikNode.lock())
				ikMap.emplace(ikNodePtr->m_name, std::move(ik));
		}
	}
	m_iks.clear();
	for (const auto& ik : vmd.m_iks) {
		for (const auto& [m_name, m_enable] : ik.m_ikInfos) {
			auto ikName = Util::SjisToUtf8(m_name);
			auto [findIt, inserted] = ikMap.try_emplace(ikName);
			auto& [first, second] = findIt->second;
			if (inserted) {
				auto it = std::ranges::find(model->ikSolvers, ikName,
					[](const std::shared_ptr<IkSolver>& ikSolver){
						const auto ikNodePtr = ikSolver->ikNode.lock();
						return ikNodePtr ? ikNodePtr->m_name : std::string{};
					}
				);
				first = it != model->ikSolvers.end() ? *it : nullptr;
			}
			if (!first)
				continue;
			auto& [keyTime, ikEnable] = second.emplace_back();
			keyTime = static_cast<int32_t>(ik.m_frame);
			ikEnable = m_enable != 0;
		}
	}
	for (auto& val : ikMap | std::views::values) {
		std::ranges::sort(val.second, {}, &IKAnimationKey::time);
		m_iks.insert(std::move(val));
	}
	std::map<std::string, std::pair<std::shared_ptr<Morph>, std::vector<MorphAnimationKey>>> morphMap;
	for (auto& morph : m_morphs) {
		if (morph.first)
			morphMap.emplace(morph.first->name, std::move(morph));
	}
	m_morphs.clear();
	for (const auto& [m_blendShapeName, m_frame, m_weight] : vmd.m_morphs) {
		auto morphName = Util::SjisToUtf8(m_blendShapeName);
		auto [findIt, inserted] = morphMap.try_emplace(morphName);
		auto& [first, second] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(model->morphs, morphName, &Morph::name);
			first = it != model->morphs.end() ? std::shared_ptr<Morph>(model, it->get()) : nullptr;
		}
		if (!first)
			continue;
		auto& [keyTime, morphWeight] = second.emplace_back();
		keyTime = static_cast<int32_t>(m_frame);
		morphWeight = m_weight;
	}
	for (auto& val : morphMap | std::views::values) {
		std::ranges::sort(val.second, {}, &MorphAnimationKey::time);
		m_morphs.insert(std::move(val));
	}
	return true;
}

void Animation::Destroy() {
	model.reset();
	m_nodes.clear();
	m_iks.clear();
	m_morphs.clear();
}

void Animation::Evaluate(const float t, const float animWeight) const {
	for (const auto& [node, keys]: m_nodes) {
		if (!node)
			continue;
		if (keys.empty()) {
			node->m_animTranslate = glm::vec3(0);
			node->m_animRotate = glm::quat(1, 0, 0, 0);
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const NodeAnimationKey& k) { return static_cast<float>(k.time); });
		const auto& cur = it != keys.end() ? *it : keys.back();
		glm::vec3 vt = cur.translate;
		glm::quat q  = cur.rotate;
		if (it != keys.begin() && it != keys.end()) {
			const auto& prev = *(it - 1);
			const auto& [keyTime, keyTranslate, keyRotate,
				keyTxBezier, keyTyBezier, keyTzBezier,
				keyRotBezier] = *it;
			const auto timeRange = static_cast<float>(keyTime - prev.time);
			const float time = (t - static_cast<float>(prev.time)) / timeRange;
			const float tx_x  = FindBezierX(time, keyTxBezier.first.x,  keyTxBezier.second.x);
			const float ty_x  = FindBezierX(time, keyTyBezier.first.x,  keyTyBezier.second.x);
			const float tz_x  = FindBezierX(time, keyTzBezier.first.x,  keyTzBezier.second.x);
			const float rot_x = FindBezierX(time, keyRotBezier.first.x, keyRotBezier.second.x);
			const float tx_y  = Bezier(tx_x,  keyTxBezier.first.y,  keyTxBezier.second.y);
			const float ty_y  = Bezier(ty_x,  keyTyBezier.first.y,  keyTyBezier.second.y);
			const float tz_y  = Bezier(tz_x,  keyTzBezier.first.y,  keyTzBezier.second.y);
			const float rot_y = Bezier(rot_x, keyRotBezier.first.y, keyRotBezier.second.y);
			vt = glm::mix(prev.translate, keyTranslate, glm::vec3(tx_y, ty_y, tz_y));
			q  = glm::slerp(prev.rotate,   keyRotate,   rot_y);
		}
		node->m_animTranslate = animWeight != 1.0f ? glm::mix(node->m_baseAnimTranslate, vt, animWeight) : vt;
		node->m_animRotate = animWeight != 1.0f ? glm::slerp(node->m_baseAnimRotate, q, animWeight) : q;
	}
	for (const auto& [ikSolver, keys] : m_iks) {
		if (!ikSolver)
			continue;
		if (keys.empty()) {
			ikSolver->enable = true;
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const IKAnimationKey& k) { return static_cast<float>(k.time); });
		const bool enable = it != keys.begin() ? (it - 1)->ikEnable : keys.begin()->ikEnable;
		ikSolver->enable = animWeight < 1.0f ? ikSolver->baseAnimEnable : enable;
	}
	for (const auto& [morph, keys] : m_morphs) {
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

bool CameraAnimation::Create(const VMDReader& vmd) {
	if (!vmd.m_cameras.empty()) {
		m_keys.clear();
		for (const auto& cam: vmd.m_cameras) {
			CameraAnimationKey key{};
			key.time = static_cast<int32_t>(cam.m_frame);
			key.interest = cam.m_interest * glm::vec3(1, 1, -1);
			key.rotate = cam.m_rotate;
			key.distance = cam.m_distance;
			key.fov = glm::radians(static_cast<float>(cam.m_viewAngle));
			AssignBezier(key.ixBezier,
				cam.m_interpolation[0], cam.m_interpolation[1],
				cam.m_interpolation[2], cam.m_interpolation[3]);
			AssignBezier(key.iyBezier,
				cam.m_interpolation[4], cam.m_interpolation[5],
				cam.m_interpolation[6], cam.m_interpolation[7]);
			AssignBezier(key.izBezier,
				cam.m_interpolation[8], cam.m_interpolation[9],
				cam.m_interpolation[10], cam.m_interpolation[11]);
			AssignBezier(key.rotateBezier,
				cam.m_interpolation[12], cam.m_interpolation[13],
				cam.m_interpolation[14], cam.m_interpolation[15]);
			AssignBezier(key.distanceBezier,
				cam.m_interpolation[16], cam.m_interpolation[17],
				cam.m_interpolation[18], cam.m_interpolation[19]);
			AssignBezier(key.fovBezier,
				cam.m_interpolation[20], cam.m_interpolation[21],
				cam.m_interpolation[22], cam.m_interpolation[23]);
			m_keys.push_back(key);
		}
		std::ranges::sort(m_keys, {}, &CameraAnimationKey::time);
	} else
		return false;
	return true;
}

void CameraAnimation::Evaluate(const float t) {
	if (m_keys.empty())
		return;
	const auto it = std::ranges::upper_bound(m_keys, t, std::less{},
		[](const CameraAnimationKey& k) { return static_cast<float>(k.time); });
	const auto& cur = it != m_keys.end() ? *it : m_keys.back();
	camera.interest = cur.interest;
	camera.rotate = cur.rotate;
	camera.distance = cur.distance;
	camera.fov = cur.fov;
	if (it == m_keys.begin() || it == m_keys.end())
		return;
	const auto& [keyTime, keyInterest, keyRotate, keyDistance, keyFov,
		keyIxBezier, keyIyBezier, keyIzBezier,
		keyRotateBezier, keyDistanceBezier, keyFovBezier] = *it;
	const auto& prev = *(it - 1);
	if (keyTime - prev.time <= 1) {
		camera.interest = prev.interest;
		camera.rotate = prev.rotate;
		camera.distance = prev.distance;
		camera.fov = prev.fov;
		return;
	}
	const float time = (t - static_cast<float>(prev.time)) / static_cast<float>(keyTime - prev.time);
	const float ix_y = Bezier(FindBezierX(time, keyIxBezier.first.x, keyIxBezier.second.x), keyIxBezier.first.y, keyIxBezier.second.y);
	const float iy_y = Bezier(FindBezierX(time, keyIyBezier.first.x, keyIyBezier.second.x), keyIyBezier.first.y, keyIyBezier.second.y);
	const float iz_y = Bezier(FindBezierX(time, keyIzBezier.first.x, keyIzBezier.second.x), keyIzBezier.first.y, keyIzBezier.second.y);
	const float r_y = Bezier(FindBezierX(time, keyRotateBezier.first.x, keyRotateBezier.second.x), keyRotateBezier.first.y, keyRotateBezier.second.y);
	const float d_y = Bezier(FindBezierX(time, keyDistanceBezier.first.x, keyDistanceBezier.second.x), keyDistanceBezier.first.y, keyDistanceBezier.second.y);
	const float f_y = Bezier(FindBezierX(time, keyFovBezier.first.x, keyFovBezier.second.x), keyFovBezier.first.y, keyFovBezier.second.y);
	camera.interest = glm::mix(prev.interest, keyInterest, glm::vec3(ix_y, iy_y, iz_y));
	camera.rotate = glm::mix(prev.rotate, keyRotate, r_y);
	camera.distance = glm::mix(prev.distance, keyDistance, d_y);
	camera.fov = glm::mix(prev.fov, keyFov, f_y);
}

