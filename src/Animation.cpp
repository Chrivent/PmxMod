#include "Animation.h"

#include "Model.h"
#include "Util.h"

#include <ranges>

std::string SjisToUtf8(const char* sjis) {
	if (!sjis)
		return {};
	const int need = MultiByteToWideChar(
		932, MB_ERR_INVALID_CHARS,
		sjis, -1,
		nullptr, 0);
	if (need <= 0)
		return {};
	std::wstring w(static_cast<size_t>(need), L'\0');
	const int written = MultiByteToWideChar(
		932, MB_ERR_INVALID_CHARS,
		sjis, -1,
		w.data(), need);
	if (written <= 0)
		return {};
	if (!w.empty() && w.back() == L'\0')
		w.pop_back();
	return Util::WStringToUtf8(w);
}

void SetBezier(std::pair<glm::vec2, glm::vec2>& bezier, const int x0, const int x1, const int y0, const int y1) {
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

bool Animation::Add(const VMDReader& vmd) {
	std::map<std::string, std::pair<Node*, std::vector<NodeAnimationKey>>> nodeMap;
	for (auto& node : m_nodes)
		nodeMap.emplace(node.first->m_name, std::move(node));
	m_nodes.clear();
	for (const auto& motion : vmd.m_motions) {
		auto nodeName = SjisToUtf8(motion.m_boneName);
		auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
		auto& [first, second] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(m_model->m_nodes, nodeName, &Node::m_name);
			first = it != m_model->m_nodes.end() ? it->get() : nullptr;
		}
		if (!first)
			continue;
		second.emplace_back().Set(motion);
	}
	for (auto& val : nodeMap | std::views::values) {
		std::ranges::sort(val.second, {}, &NodeAnimationKey::m_time);
		m_nodes.insert(std::move(val));
	}
	std::map<std::string, std::pair<IkSolver*, std::vector<IKAnimationKey>>> ikMap;
	for (auto& ik : m_iks)
		ikMap.emplace(ik.first->m_ikNode->m_name, std::move(ik));
	m_iks.clear();
	for (const auto& ik : vmd.m_iks) {
		for (const auto& [m_name, m_enable] : ik.m_ikInfos) {
			auto ikName = SjisToUtf8(m_name);
			auto [findIt, inserted] = ikMap.try_emplace(ikName);
			auto& [first, second] = findIt->second;
			if (inserted) {
				auto it = std::ranges::find(
					m_model->m_ikSolvers, ikName,
					[](const std::unique_ptr<IkSolver>& ikSolver){ return ikSolver->m_ikNode->m_name; }
				);
				first = it != m_model->m_ikSolvers.end() ? it->get() : nullptr;
			}
			if (!first)
				continue;
			auto& [m_time, m_ikEnable] = second.emplace_back();
			m_time = static_cast<int32_t>(ik.m_frame);
			m_ikEnable = m_enable != 0;
		}
	}
	for (auto& val : ikMap | std::views::values) {
		std::ranges::sort(val.second, {}, &IKAnimationKey::m_time);
		m_iks.insert(std::move(val));
	}
	std::map<std::string, std::pair<Morph*, std::vector<MorphAnimationKey>>> morphMap;
	for (auto& morph : m_morphs)
		morphMap.emplace(morph.first->m_name, std::move(morph));
	m_morphs.clear();
	for (const auto& [m_blendShapeName, m_frame, m_weight] : vmd.m_morphs) {
		auto morphName = SjisToUtf8(m_blendShapeName);
		auto [findIt, inserted] = morphMap.try_emplace(morphName);
		auto& [first, second] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(m_model->m_morphs, morphName, &Morph::m_name);
			first = it != m_model->m_morphs.end() ? it->get() : nullptr;
		}
		if (!first)
			continue;
		auto& [m_time, m_morphWeight] = second.emplace_back();
		m_time = static_cast<int32_t>(m_frame);
		m_morphWeight = m_weight;
	}
	for (auto& val : morphMap | std::views::values) {
		std::ranges::sort(val.second, {}, &MorphAnimationKey::m_time);
		m_morphs.insert(std::move(val));
	}
	return true;
}

void Animation::Destroy() {
	m_model.reset();
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
			[](const NodeAnimationKey& k) { return static_cast<float>(k.m_time); });
		const auto& cur = it != keys.end() ? *it : keys.back();
		glm::vec3 vt = cur.m_translate;
		glm::quat q  = cur.m_rotate;
		if (it != keys.begin() && it != keys.end()) {
			const auto& prev = *(it - 1);
			const auto& [m_time, m_translate, m_rotate,
						 m_txBezier, m_tyBezier, m_tzBezier,
						 m_rotBezier] = *it;
			const auto timeRange = static_cast<float>(m_time - prev.m_time);
			const float time = (t - static_cast<float>(prev.m_time)) / timeRange;
			const float tx_x  = FindBezierX(time, m_txBezier.first.x,  m_txBezier.second.x);
			const float ty_x  = FindBezierX(time, m_tyBezier.first.x,  m_tyBezier.second.x);
			const float tz_x  = FindBezierX(time, m_tzBezier.first.x,  m_tzBezier.second.x);
			const float rot_x = FindBezierX(time, m_rotBezier.first.x, m_rotBezier.second.x);
			const float tx_y  = Bezier(tx_x,  m_txBezier.first.y,  m_txBezier.second.y);
			const float ty_y  = Bezier(ty_x,  m_tyBezier.first.y,  m_tyBezier.second.y);
			const float tz_y  = Bezier(tz_x,  m_tzBezier.first.y,  m_tzBezier.second.y);
			const float rot_y = Bezier(rot_x, m_rotBezier.first.y, m_rotBezier.second.y);
			vt = glm::mix(prev.m_translate, m_translate, glm::vec3(tx_y, ty_y, tz_y));
			q  = glm::slerp(prev.m_rotate,   m_rotate,   rot_y);
		}
		node->m_animTranslate = animWeight != 1.0f ? glm::mix(node->m_baseAnimTranslate, vt, animWeight) : vt;
		node->m_animRotate = animWeight != 1.0f ? glm::slerp(node->m_baseAnimRotate, q, animWeight) : q;
	}
	for (const auto& [ikSolver, keys] : m_iks) {
		if (!ikSolver)
			continue;
		if (keys.empty()) {
			ikSolver->m_enable = true;
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const IKAnimationKey& k) { return static_cast<float>(k.m_time); });
		const bool enable = it != keys.begin() ? (it - 1)->m_ikEnable : keys.begin()->m_ikEnable;
		ikSolver->m_enable = animWeight < 1.0f ? ikSolver->m_baseAnimEnable : enable;
	}
	for (const auto& [morph, keys] : m_morphs) {
		if (!morph)
			continue;
		if (keys.empty())
			continue;
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const MorphAnimationKey& k) { return static_cast<float>(k.m_time); });
		float weight = it != keys.end() ? it->m_morphWeight : keys.back().m_morphWeight;
		if (it != keys.begin() && it != keys.end()) {
			auto [m_time0, m_weight0] = *(it - 1);
			auto [m_time1, m_weight1] = *it;
			const float time = (t - static_cast<float>(m_time0)) / static_cast<float>(m_time1 - m_time0);
			weight = (m_weight1 - m_weight0) * time + m_weight0;
		}
		morph->m_weight = animWeight != 1.0f ? glm::mix(morph->m_saveAnimWeight, weight, animWeight) : weight;
	}
}

void NodeAnimationKey::Set(const VMDReader::VMDMotion& motion) {
	m_time = static_cast<int32_t>(motion.m_frame);
	m_translate = motion.m_translate * glm::vec3(1, 1, -1);
	const glm::quat q = motion.m_quaternion;
	const auto rot = Util::InvZ(glm::mat3_cast(q));
	m_rotate = glm::quat_cast(rot);
	SetBezier(m_txBezier,
		motion.m_interpolation[0], motion.m_interpolation[8],
		motion.m_interpolation[4], motion.m_interpolation[12]);
	SetBezier(m_tyBezier,
		motion.m_interpolation[1], motion.m_interpolation[9],
		motion.m_interpolation[5], motion.m_interpolation[13]);
	SetBezier(m_tzBezier,
		motion.m_interpolation[2], motion.m_interpolation[10],
		motion.m_interpolation[6], motion.m_interpolation[14]);
	SetBezier(m_rotBezier,
		motion.m_interpolation[3], motion.m_interpolation[11],
		motion.m_interpolation[7], motion.m_interpolation[15]);
}

glm::mat4 Camera::GetViewMatrix() const {
	glm::mat4 view(1.0f);
	view = glm::translate(view, glm::vec3(0, 0, -m_distance));
	glm::mat4 rot(1.0f);
	rot = glm::rotate(rot, m_rotate.y, glm::vec3(0, 1, 0));
	rot = glm::rotate(rot, m_rotate.z, glm::vec3(0, 0, -1));
	rot = glm::rotate(rot, m_rotate.x, glm::vec3(1, 0, 0));
	view = rot * view;
	const glm::vec3 eye = glm::vec3(view[3]) + m_interest;
	const glm::vec3 center = glm::mat3(view) * glm::vec3(0, 0, -1) + eye;
	const glm::vec3 up = glm::mat3(view) * glm::vec3(0, 1, 0);
	return glm::lookAt(eye, center, up);
}

bool CameraAnimation::Create(const VMDReader& vmd) {
	if (!vmd.m_cameras.empty()) {
		m_keys.clear();
		for (const auto& cam: vmd.m_cameras) {
			CameraAnimationKey key{};
			key.m_time = static_cast<int32_t>(cam.m_frame);
			key.m_interest = cam.m_interest * glm::vec3(1, 1, -1);
			key.m_rotate = cam.m_rotate;
			key.m_distance = cam.m_distance;
			key.m_fov = glm::radians(static_cast<float>(cam.m_viewAngle));
			SetBezier(key.m_ixBezier,
				cam.m_interpolation[0], cam.m_interpolation[1],
				cam.m_interpolation[2], cam.m_interpolation[3]);
			SetBezier(key.m_iyBezier,
				cam.m_interpolation[4], cam.m_interpolation[5],
				cam.m_interpolation[6], cam.m_interpolation[7]);
			SetBezier(key.m_izBezier,
				cam.m_interpolation[8], cam.m_interpolation[9],
				cam.m_interpolation[10], cam.m_interpolation[11]);
			SetBezier(key.m_rotateBezier,
				cam.m_interpolation[12], cam.m_interpolation[13],
				cam.m_interpolation[14], cam.m_interpolation[15]);
			SetBezier(key.m_distanceBezier,
				cam.m_interpolation[16], cam.m_interpolation[17],
				cam.m_interpolation[18], cam.m_interpolation[19]);
			SetBezier(key.m_fovBezier,
				cam.m_interpolation[20], cam.m_interpolation[21],
				cam.m_interpolation[22], cam.m_interpolation[23]);
			m_keys.push_back(key);
		}
		std::ranges::sort(m_keys, {}, &CameraAnimationKey::m_time);
	} else
		return false;
	return true;
}

void CameraAnimation::Evaluate(const float t) {
	if (m_keys.empty())
		return;
	const auto it = std::ranges::upper_bound(m_keys, t, std::less{},
		[](const CameraAnimationKey& k) { return static_cast<float>(k.m_time); });
	const auto& cur = it != m_keys.end() ? *it : m_keys.back();
	m_camera.m_interest = cur.m_interest;
	m_camera.m_rotate = cur.m_rotate;
	m_camera.m_distance = cur.m_distance;
	m_camera.m_fov = cur.m_fov;
	if (it == m_keys.begin() || it == m_keys.end())
		return;
	const auto& [m_time, m_interest, m_rotate, m_distance, m_fov,
		m_ixBezier, m_iyBezier, m_izBezier,
		m_rotateBezier, m_distanceBezier, m_fovBezier] = *it;
	const auto& prev = *(it - 1);
	if (m_time - prev.m_time <= 1) {
		m_camera.m_interest = prev.m_interest;
		m_camera.m_rotate = prev.m_rotate;
		m_camera.m_distance = prev.m_distance;
		m_camera.m_fov = prev.m_fov;
		return;
	}
	const float time = (t - static_cast<float>(prev.m_time)) / static_cast<float>(m_time - prev.m_time);
	const float ix_y = Bezier(FindBezierX(time, m_ixBezier.first.x, m_ixBezier.second.x), m_ixBezier.first.y, m_ixBezier.second.y);
	const float iy_y = Bezier(FindBezierX(time, m_iyBezier.first.x, m_iyBezier.second.x), m_iyBezier.first.y, m_iyBezier.second.y);
	const float iz_y = Bezier(FindBezierX(time, m_izBezier.first.x, m_izBezier.second.x), m_izBezier.first.y, m_izBezier.second.y);
	const float r_y = Bezier(FindBezierX(time, m_rotateBezier.first.x, m_rotateBezier.second.x), m_rotateBezier.first.y, m_rotateBezier.second.y);
	const float d_y = Bezier(FindBezierX(time, m_distanceBezier.first.x, m_distanceBezier.second.x), m_distanceBezier.first.y, m_distanceBezier.second.y);
	const float f_y = Bezier(FindBezierX(time, m_fovBezier.first.x, m_fovBezier.second.x), m_fovBezier.first.y, m_fovBezier.second.y);
	m_camera.m_interest = glm::mix(prev.m_interest, m_interest, glm::vec3(ix_y, iy_y, iz_y));
	m_camera.m_rotate = glm::mix(prev.m_rotate, m_rotate, r_y);
	m_camera.m_distance = glm::mix(prev.m_distance, m_distance, d_y);
	m_camera.m_fov = glm::mix(prev.m_fov, m_fov, f_y);
}
