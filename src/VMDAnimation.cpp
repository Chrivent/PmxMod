#include "VMDAnimation.h"

#include "MMDNode.h"
#include "MMDModel.h"
#include "MMDIkSolver.h"

#include "MMDUtil.h"

#include <algorithm>
#include <map>
#include <ranges>

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
	for (int i = 0; i < 32; i++) {
		const float x = Bezier(t, x1, x2);
		const float diff = time - x;
		if (std::abs(diff) < 1e-5f)
			break;
		(diff < 0.0f ? stop : start) = t;
		t = (start + stop) * 0.5f;
	}
	return t;
}

bool VMDAnimation::Add(const VMDReader& vmd) {
	std::map<std::string, std::pair<MMDNode*, std::vector<VMDNodeAnimationKey>>> nodeMap;
	for (auto& node : m_nodes)
		nodeMap.emplace(node.first->m_name, std::move(node));
	m_nodes.clear();
	for (const auto& motion : vmd.m_motions) {
		std::string nodeName = UnicodeUtil::SjisToUtf8(motion.m_boneName);
		auto findIt = nodeMap.find(nodeName);
		std::pair<MMDNode*, std::vector<VMDNodeAnimationKey>>* nodePair = nullptr;
		if (findIt == std::end(nodeMap)) {
			auto it = std::ranges::find(
				m_model->m_nodes, std::string_view{ nodeName },
				[](const std::unique_ptr<MMDNode>& node) -> std::string_view { return node->m_name; }
			);
			auto* node = it == m_model->m_nodes.end() ? nullptr : it->get();
			if (node != nullptr)
				nodePair = &nodeMap.emplace(nodeName,
					std::pair<MMDNode*, std::vector<VMDNodeAnimationKey>>{ node, {} }).first->second;
		} else
			nodePair = &findIt->second;
		if (nodePair != nullptr) {
			VMDNodeAnimationKey key{};
			key.Set(motion);
			nodePair->second.push_back(key);
		}
	}
	for (auto& val : nodeMap | std::views::values) {
		std::ranges::sort(val.second, {}, &VMDNodeAnimationKey::m_time);
		m_nodes.insert(std::move(val));
	}
	std::map<std::string, std::pair<MMDIkSolver*, std::vector<VMDIKAnimationKey>>> ikMap;
	for (auto& ik : m_iks)
		ikMap.emplace(ik.first->m_ikNode->m_name, std::move(ik));
	m_iks.clear();
	for (const auto& ik : vmd.m_iks) {
		for (const auto& [m_name, m_enable] : ik.m_ikInfos) {
			std::string ikName = UnicodeUtil::SjisToUtf8(m_name);
			auto findIt = ikMap.find(ikName);
			std::pair<MMDIkSolver*, std::vector<VMDIKAnimationKey>>* ikPair = nullptr;
			if (findIt == std::end(ikMap)) {
				auto it = std::ranges::find(
					m_model->m_ikSolvers, std::string_view{ ikName },
					[](const std::unique_ptr<MMDIkSolver>& ikSolver) -> std::string_view { return ikSolver->m_ikNode->m_name; }
				);
				auto* ikSolver = it == m_model->m_ikSolvers.end() ? nullptr : it->get();
				if (ikSolver != nullptr)
					ikPair = &ikMap.emplace(ikName,
						std::pair<MMDIkSolver*, std::vector<VMDIKAnimationKey>>{ ikSolver, {} }).first->second;
			} else
				ikPair = &findIt->second;
			if (ikPair != nullptr) {
				VMDIKAnimationKey key{};
				key.m_time = static_cast<int32_t>(ik.m_frame);
				key.m_enable = m_enable != 0;
				ikPair->second.push_back(key);
			}
		}
	}
	for (auto& val : ikMap | std::views::values) {
		std::ranges::sort(val.second, {}, &VMDIKAnimationKey::m_time);
		m_iks.insert(std::move(val));
	}
	std::map<std::string, std::pair<MMDMorph*, std::vector<VMDMorphAnimationKey>>> morphMap;
	for (auto& morph : m_morphs)
		morphMap.emplace(morph.first->m_name, std::move(morph));
	m_morphs.clear();
	for (const auto& [m_blendShapeName, m_frame, m_weight] : vmd.m_morphs) {
		std::string morphName = UnicodeUtil::SjisToUtf8(m_blendShapeName);
		auto findIt = morphMap.find(morphName);
		std::pair<MMDMorph*, std::vector<VMDMorphAnimationKey>>* morphPair = nullptr;
		if (findIt == std::end(morphMap)) {
			auto it = std::ranges::find(
				m_model->m_morphs, std::string_view{ morphName },
				[](const std::unique_ptr<MMDMorph>& mmdMorph) -> std::string_view { return mmdMorph->m_name; }
			);
			auto* mmdMorph = it == m_model->m_morphs.end() ? nullptr : it->get();
			if (mmdMorph != nullptr)
				morphPair = &morphMap.emplace(morphName,
						std::pair<MMDMorph*, std::vector<VMDMorphAnimationKey>>{ mmdMorph, {} }).first->second;
		} else
			morphPair = &findIt->second;
		if (morphPair != nullptr) {
			VMDMorphAnimationKey key{};
			key.m_time = static_cast<int32_t>(m_frame);
			key.m_weight = m_weight;
			morphPair->second.push_back(key);
		}
	}
	for (auto& val : morphMap | std::views::values) {
		std::ranges::sort(val.second, {}, &VMDMorphAnimationKey::m_time);
		m_morphs.insert(std::move(val));
	}
	return true;
}

void VMDAnimation::Destroy() {
	m_model.reset();
	m_nodes.clear();
	m_iks.clear();
	m_morphs.clear();
}

void VMDAnimation::Evaluate(const float t, const float animWeight) const {
	for (const auto& [node, keys]: m_nodes) {
		if (node == nullptr)
			return;
		if (keys.empty()) {
			node->m_animTranslate = glm::vec3(0);
			node->m_animRotate = glm::quat(1, 0, 0, 0);
			return;
		}
		const auto boundIt = std::ranges::upper_bound(keys, t, std::less{},
			[](const VMDNodeAnimationKey& k) { return static_cast<float>(k.m_time); });
		glm::vec3 vt;
		glm::quat q;
		if (boundIt == std::end(keys)) {
			vt = keys.back().m_translate;
			q = keys.back().m_rotate;
		} else {
			vt = boundIt->m_translate;
			q = boundIt->m_rotate;
			if (boundIt != std::begin(keys)) {
				const auto& key = *(boundIt - 1);
				const auto& [m_time, m_translate, m_rotate
							, m_txBezier, m_tyBezier, m_tzBezier, m_rotBezier]
						= *boundIt;
				const auto timeRange = static_cast<float>(m_time - key.m_time);
				const float time = (t - static_cast<float>(key.m_time)) / timeRange;
				const float tx_x = FindBezierX(time, m_txBezier.first.x, m_txBezier.second.x);
				const float ty_x = FindBezierX(time, m_tyBezier.first.x, m_tyBezier.second.x);
				const float tz_x = FindBezierX(time, m_tzBezier.first.x, m_tzBezier.second.x);
				const float rot_x = FindBezierX(time, m_rotBezier.first.x, m_rotBezier.second.x);
				const float tx_y = Bezier(tx_x, m_txBezier.first.y, m_txBezier.second.y);
				const float ty_y = Bezier(ty_x, m_tyBezier.first.y, m_tyBezier.second.y);
				const float tz_y = Bezier(tz_x, m_tzBezier.first.y, m_tzBezier.second.y);
				const float rot_y = Bezier(rot_x, m_rotBezier.first.y, m_rotBezier.second.y);
				vt = glm::mix(key.m_translate, m_translate, glm::vec3(tx_y, ty_y, tz_y));
				q = glm::slerp(key.m_rotate, m_rotate, rot_y);
			}
		}
		if (animWeight == 1.0f) {
			node->m_animRotate = q;
			node->m_animTranslate = vt;
		} else {
			const auto baseQ = node->m_baseAnimRotate;
			const auto baseT = node->m_baseAnimTranslate;
			node->m_animRotate = glm::slerp(baseQ, q, animWeight);
			node->m_animTranslate = glm::mix(baseT, vt, animWeight);
		}
	}
	for (const auto& [ikSolver, keys] : m_iks) {
		if (ikSolver == nullptr)
			return;
		if (keys.empty()) {
			ikSolver->m_enable = true;
			return;
		}
		const auto boundIt = std::ranges::upper_bound(keys, t, std::less{},
			[](const VMDIKAnimationKey& k) { return static_cast<float>(k.m_time); });
		bool enable;
		if (boundIt == std::end(keys))
			enable = keys.rbegin()->m_enable;
		else {
			enable = keys.begin()->m_enable;
			if (boundIt != std::begin(keys)) {
				const auto& [m_time, m_enable] = *(boundIt - 1);
				enable = m_enable;
			}
		}
		if (animWeight != 1.0f && animWeight < 1.0f)
			ikSolver->m_enable = ikSolver->m_baseAnimEnable;
		else
			ikSolver->m_enable = enable;
	}
	for (const auto& [morph, keys] : m_morphs) {
		if (morph == nullptr)
			return;
		if (keys.empty())
			return;
		float weight;
		const auto boundIt = std::ranges::upper_bound(keys, t, std::less{},
			[](const VMDMorphAnimationKey& k) { return static_cast<float>(k.m_time); });
		if (boundIt == std::end(keys))
			weight = keys.rbegin()->m_weight;
		else {
			weight = boundIt->m_weight;
			if (boundIt != std::begin(keys)) {
				auto [m_time0, m_weight0] = *(boundIt - 1);
				auto [m_time1, m_weight1] = *boundIt;
				const auto timeRange = static_cast<float>(m_time1 - m_time0);
				const float time = (t - static_cast<float>(m_time0)) / timeRange;
				weight = (m_weight1 - m_weight0) * time + m_weight0;
			}
		}
		if (animWeight == 1.0f)
			morph->m_weight = weight;
		else
			morph->m_weight = glm::mix(morph->m_saveAnimWeight, weight, animWeight);
	}
}

void VMDNodeAnimationKey::Set(const VMDReader::VMDMotion& motion) {
	m_time = static_cast<int32_t>(motion.m_frame);
	m_translate = motion.m_translate * glm::vec3(1, 1, -1);
	const glm::quat q = motion.m_quaternion;
	const auto invZ = glm::mat3(glm::scale(glm::mat4(1), glm::vec3(1, 1, -1)));
	const auto rot0 = glm::mat3_cast(q);
	const auto rot1 = invZ * rot0 * invZ;
	m_rotate = glm::quat_cast(rot1);
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

MMDCamera::MMDCamera() {
	m_interest = glm::vec3(0, 10, 0);
	m_rotate = glm::vec3(0, 0, 0);
	m_distance = 50;
	m_fov = glm::radians(30.0f);
}

glm::mat4 MMDCamera::GetViewMatrix() const {
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

bool VMDCameraAnimation::Create(const VMDReader& vmd) {
	if (!vmd.m_cameras.empty()) {
		m_keys.clear();
		for (const auto& cam: vmd.m_cameras) {
			VMDCameraAnimationKey key{};
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
		std::ranges::sort(m_keys, {}, &VMDCameraAnimationKey::m_time);
	} else
		return false;
	return true;
}

void VMDCameraAnimation::Evaluate(const float t) {
	auto Apply = [&](const VMDCameraAnimationKey& k){
		m_camera.m_interest = k.m_interest;
		m_camera.m_rotate   = k.m_rotate;
		m_camera.m_distance = k.m_distance;
		m_camera.m_fov      = k.m_fov;
	};
	if (m_keys.empty())
		return;
	const auto boundIt = std::ranges::upper_bound(m_keys, t, std::less{},
		[](const VMDCameraAnimationKey& k) { return static_cast<float>(k.m_time); });
	if (boundIt == std::end(m_keys))
		Apply(m_keys.back());
	else {
		Apply(*boundIt);
		if (boundIt != std::begin(m_keys)) {
			const auto& key = *(boundIt - 1);
			const auto& [m_time, m_interest, m_rotate, m_distance, m_fov
						, m_ixBezier, m_iyBezier, m_izBezier, m_rotateBezier, m_distanceBezier, m_fovBezier]
					= *boundIt;
			if (m_time - key.m_time > 1) {
				const auto timeRange = static_cast<float>(m_time - key.m_time);
				const float time = (t - static_cast<float>(key.m_time)) / timeRange;
				const float ix_x = FindBezierX(time, m_ixBezier.first.x, m_ixBezier.second.x);
				const float iy_x = FindBezierX(time, m_iyBezier.first.x, m_iyBezier.second.x);
				const float iz_x = FindBezierX(time, m_izBezier.first.x, m_izBezier.second.x);
				const float rotate_x = FindBezierX(time, m_rotateBezier.first.x, m_rotateBezier.second.x);
				const float distance_x = FindBezierX(time, m_distanceBezier.first.x, m_distanceBezier.second.x);
				const float fov_x = FindBezierX(time, m_fovBezier.first.x, m_fovBezier.second.x);
				const float ix_y = Bezier(ix_x, m_ixBezier.first.y, m_ixBezier.second.y);
				const float iy_y = Bezier(iy_x, m_iyBezier.first.y, m_iyBezier.second.y);
				const float iz_y = Bezier(iz_x, m_izBezier.first.y, m_izBezier.second.y);
				const float rotate_y = Bezier(rotate_x, m_rotateBezier.first.y, m_rotateBezier.second.y);
				const float distance_y = Bezier(distance_x, m_distanceBezier.first.y, m_distanceBezier.second.y);
				const float fov_y = Bezier(fov_x, m_fovBezier.first.y, m_fovBezier.second.y);
				m_camera.m_interest = glm::mix(key.m_interest, m_interest, glm::vec3(ix_y, iy_y, iz_y));
				m_camera.m_rotate = glm::mix(key.m_rotate, m_rotate, rotate_y);
				m_camera.m_distance = glm::mix(key.m_distance, m_distance, distance_y);
				m_camera.m_fov = glm::mix(key.m_fov, m_fov, fov_y);
			} else
				Apply(key);
		}
	}
}
