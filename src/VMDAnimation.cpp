#include "VMDAnimation.h"

#include "MMDNode.h"
#include "MMDModel.h"
#include "MMDIkSolver.h"

#include "MMDUtil.h"

#include <algorithm>
#include <map>
#include <ranges>

void SetVMDBezier(VMDBezier& bezier, const int x0, const int x1, const int y0, const int y1) {
	bezier.m_cp1 = glm::vec2(static_cast<float>(x0) / 127.0f, static_cast<float>(y0) / 127.0f);
	bezier.m_cp2 = glm::vec2(static_cast<float>(x1) / 127.0f, static_cast<float>(y1) / 127.0f);
}

float Eval(const float t, const float p1, const float p2) {
	const float it = 1.0f - t;
	return 3.0f * it * it * t * p1 + 3.0f * it * t * t * p2 + t * t * t;
}

float VMDBezier::FindBezierX(float time) const {
	time = std::clamp(time, 0.0f, 1.0f);
	float start = 0.0f, stop = 1.0f;
	float t = 0.5f;
	for (int i = 0; i < 32; i++) {
		const float x = Eval(t, m_cp1.x, m_cp2.x);
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
				const float tx_x = m_txBezier.FindBezierX(time);
				const float ty_x = m_tyBezier.FindBezierX(time);
				const float tz_x = m_tzBezier.FindBezierX(time);
				const float rot_x = m_rotBezier.FindBezierX(time);
				const float tx_y = Eval(tx_x, m_txBezier.m_cp1.y, m_txBezier.m_cp2.y);
				const float ty_y = Eval(ty_x, m_tyBezier.m_cp1.y, m_tyBezier.m_cp2.y);
				const float tz_y = Eval(tz_x, m_tzBezier.m_cp1.y, m_tzBezier.m_cp2.y);
				const float rot_y = Eval(rot_x, m_rotBezier.m_cp1.y, m_rotBezier.m_cp2.y);
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
	SetVMDBezier(m_txBezier,
		motion.m_interpolation[0], motion.m_interpolation[8],
		motion.m_interpolation[4], motion.m_interpolation[12]);
	SetVMDBezier(m_tyBezier,
		motion.m_interpolation[1], motion.m_interpolation[9],
		motion.m_interpolation[5], motion.m_interpolation[13]);
	SetVMDBezier(m_tzBezier,
		motion.m_interpolation[2], motion.m_interpolation[10],
		motion.m_interpolation[6], motion.m_interpolation[14]);
	SetVMDBezier(m_rotBezier,
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
			SetVMDBezier(key.m_ixBezier,
				cam.m_interpolation[0], cam.m_interpolation[1],
				cam.m_interpolation[2], cam.m_interpolation[3]);
			SetVMDBezier(key.m_iyBezier,
				cam.m_interpolation[4], cam.m_interpolation[5],
				cam.m_interpolation[6], cam.m_interpolation[7]);
			SetVMDBezier(key.m_izBezier,
				cam.m_interpolation[8], cam.m_interpolation[9],
				cam.m_interpolation[10], cam.m_interpolation[11]);
			SetVMDBezier(key.m_rotateBezier,
				cam.m_interpolation[12], cam.m_interpolation[13],
				cam.m_interpolation[14], cam.m_interpolation[15]);
			SetVMDBezier(key.m_distanceBezier,
				cam.m_interpolation[16], cam.m_interpolation[17],
				cam.m_interpolation[18], cam.m_interpolation[19]);
			SetVMDBezier(key.m_fovBezier,
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
				const float ix_x = m_ixBezier.FindBezierX(time);
				const float iy_x = m_iyBezier.FindBezierX(time);
				const float iz_x = m_izBezier.FindBezierX(time);
				const float rotate_x = m_rotateBezier.FindBezierX(time);
				const float distance_x = m_distanceBezier.FindBezierX(time);
				const float fov_x = m_fovBezier.FindBezierX(time);
				const float ix_y = Eval(ix_x, m_ixBezier.m_cp1.y, m_ixBezier.m_cp2.y);
				const float iy_y = Eval(iy_x, m_iyBezier.m_cp1.y, m_iyBezier.m_cp2.y);
				const float iz_y = Eval(iz_x, m_izBezier.m_cp1.y, m_izBezier.m_cp2.y);
				const float rotate_y = Eval(rotate_x, m_rotateBezier.m_cp1.y, m_rotateBezier.m_cp2.y);
				const float distance_y = Eval(distance_x, m_distanceBezier.m_cp1.y, m_distanceBezier.m_cp2.y);
				const float fov_y = Eval(fov_x, m_fovBezier.m_cp1.y, m_fovBezier.m_cp2.y);
				m_camera.m_interest = glm::mix(key.m_interest, m_interest, glm::vec3(ix_y, iy_y, iz_y));
				m_camera.m_rotate = glm::mix(key.m_rotate, m_rotate, rotate_y);
				m_camera.m_distance = glm::mix(key.m_distance, m_distance, distance_y);
				m_camera.m_fov = glm::mix(key.m_fov, m_fov, fov_y);
			} else
				Apply(key);
		}
	}
}
