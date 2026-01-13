#include "VMDAnimation.h"

#include "MMDNode.h"
#include "MMDModel.h"
#include "MMDIkSolver.h"

#include "MMDUtil.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <ranges>
#include <glm/gtc/matrix_transform.hpp>

template <typename KeyType>
std::vector<KeyType>::const_iterator FindBoundKey(const std::vector<KeyType>& keys, int32_t t, size_t startIdx) {
	if (keys.empty() || keys.size() <= startIdx)
		return keys.end();
	const auto &key0 = keys[startIdx];
	if (key0.m_time <= t) {
		if (startIdx + 1 < keys.size()) {
			const auto &key1 = keys[startIdx + 1];
			if (key1.m_time > t)
				return keys.begin() + startIdx + 1;
		} else
			return keys.end();
	} else if (startIdx != 0) {
		const auto &key1 = keys[startIdx - 1];
		if (key1.m_time <= t)
			return keys.begin() + startIdx;
	} else
		return keys.begin();
	auto bundIt = std::upper_bound(keys.begin(), keys.end(), t,
		[](int32_t lhs, const KeyType &rhs)
		{ return lhs < rhs.m_time; }
	);
	return bundIt;
}

void SetVMDBezier(VMDBezier& bezier, const int x0, const int x1, const int y0, const int y1) {
	bezier.m_cp1 = glm::vec2(static_cast<float>(x0) / 127.0f, static_cast<float>(y0) / 127.0f);
	bezier.m_cp2 = glm::vec2(static_cast<float>(x1) / 127.0f, static_cast<float>(y1) / 127.0f);
}

float Eval(const float t, const float p1, const float p2) {
	const float it = 1.0f - t;
	return 3.0f * it * it * t * p1 + 3.0f * it * t * t * p2 + t * t * t;
}

float VMDBezier::FindBezierX(const float time) const {
	constexpr float e = 0.00001f;
	float start = 0.0f;
	float stop = 1.0f;
	float t = 0.5f;
	float x = Eval(t, m_cp1.x, m_cp2.x);
	while (std::abs(time - x) > e) {
		if (time < x)
			stop = t;
		else
			start = t;
		t = (stop + start) * 0.5f;
		x = Eval(t, m_cp1.x, m_cp2.x);
	}
	return t;
}

VMDNodeController::VMDNodeController()
	: m_node(nullptr)
	, m_startKeyIndex(0) {
}

void VMDNodeController::Evaluate(const float t, const float animWeight) {
	if (m_node == nullptr)
		return;
	if (m_keys.empty()) {
		m_node->m_animTranslate = glm::vec3(0);
		m_node->m_animRotate = glm::quat(1, 0, 0, 0);
		return;
	}
	const auto boundIt = FindBoundKey(m_keys, static_cast<int32_t>(t), m_startKeyIndex);
	glm::vec3 vt;
	glm::quat q;
	if (boundIt == std::end(m_keys)) {
		vt = m_keys[m_keys.size() - 1].m_translate;
		q = m_keys[m_keys.size() - 1].m_rotate;
	} else {
		vt = boundIt->m_translate;
		q = boundIt->m_rotate;
		if (boundIt != std::begin(m_keys)) {
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
			m_startKeyIndex = std::distance(m_keys.cbegin(), boundIt);
		}
	}
	if (animWeight == 1.0f) {
		m_node->m_animRotate = q;
		m_node->m_animTranslate = vt;
	} else {
		const auto baseQ = m_node->m_baseAnimRotate;
		const auto baseT = m_node->m_baseAnimTranslate;
		m_node->m_animRotate = glm::slerp(baseQ, q, animWeight);
		m_node->m_animTranslate = glm::mix(baseT, vt, animWeight);
	}
}

VMDAnimation::VMDAnimation()
	: m_maxKeyTime(0) {
}

bool VMDAnimation::Add(const VMDReader& vmd) {
	std::map<std::string, std::unique_ptr<VMDNodeController> > nodeCtrlMap;
	for (auto& nodeCtrl : m_nodeControllers) {
		std::string name = nodeCtrl->m_node->m_name;
		nodeCtrlMap.emplace(std::make_pair(name, std::move(nodeCtrl)));
	}
	m_nodeControllers.clear();
	for (const auto& motion : vmd.m_motions) {
		std::string nodeName = UnicodeUtil::SjisToUtf8(motion.m_boneName);
		auto findIt = nodeCtrlMap.find(nodeName);
		VMDNodeController* nodeCtrl = nullptr;
		if (findIt == std::end(nodeCtrlMap)) {
			auto it = std::ranges::find(
				m_model->m_nodes, std::string_view{ nodeName },
				[](const std::unique_ptr<MMDNode>& node) -> std::string_view { return node->m_name; }
			);
			auto* node = it == m_model->m_nodes.end() ? nullptr : it->get();
			if (node != nullptr) {
				auto val = std::make_pair(
					nodeName,
					std::make_unique<VMDNodeController>()
				);
				nodeCtrl = val.second.get();
				nodeCtrl->m_node = node;
				nodeCtrlMap.emplace(std::move(val));
			}
		} else
			nodeCtrl = findIt->second.get();
		if (nodeCtrl != nullptr) {
			VMDNodeAnimationKey key{};
			key.Set(motion);
			nodeCtrl->m_keys.push_back(key);
		}
	}
	m_nodeControllers.reserve(nodeCtrlMap.size());
	for (auto& val : nodeCtrlMap | std::views::values) {
		std::ranges::sort(val->m_keys, {}, &VMDNodeAnimationKey::m_time);
		m_nodeControllers.emplace_back(std::move(val));
	}
	nodeCtrlMap.clear();
	std::map<std::string, std::unique_ptr<VMDIKController> > ikCtrlMap;
	for (auto& ikCtrl : m_ikControllers) {
		std::string name = ikCtrl->m_ikSolver->m_ikNode->m_name;
		ikCtrlMap.emplace(std::make_pair(name, std::move(ikCtrl)));
	}
	m_ikControllers.clear();
	for (const auto& ik : vmd.m_iks) {
		for (const auto& [m_name, m_enable] : ik.m_ikInfos) {
			std::string ikName = UnicodeUtil::SjisToUtf8(m_name);
			auto findIt = ikCtrlMap.find(ikName);
			VMDIKController* ikCtrl = nullptr;
			if (findIt == std::end(ikCtrlMap)) {
				auto it = std::ranges::find(
					m_model->m_ikSolvers, std::string_view{ ikName },
					[](const std::unique_ptr<MMDIkSolver>& ikSolver) -> std::string_view {
						return ikSolver->m_ikNode->m_name;
					}
				);
				auto* ikSolver = it == m_model->m_ikSolvers.end() ? nullptr : it->get();
				if (ikSolver != nullptr) {
					auto val = std::make_pair(
						ikName,
						std::make_unique<VMDIKController>()
					);
					ikCtrl = val.second.get();
					ikCtrl->m_ikSolver = ikSolver;
					ikCtrlMap.emplace(std::move(val));
				}
			} else
				ikCtrl = findIt->second.get();
			if (ikCtrl != nullptr) {
				VMDIKAnimationKey key{};
				key.m_time = static_cast<int32_t>(ik.m_frame);
				key.m_enable = m_enable != 0;
				ikCtrl->m_keys.push_back(key);
			}
		}
	}
	m_ikControllers.reserve(ikCtrlMap.size());
	for (auto& val : ikCtrlMap | std::views::values) {
		std::ranges::sort(val->m_keys, {}, &VMDIKAnimationKey::m_time);
		m_ikControllers.emplace_back(std::move(val));
	}
	ikCtrlMap.clear();
	std::map<std::string, std::unique_ptr<VMDMorphController> > morphCtrlMap;
	for (auto& morphCtrl : m_morphControllers) {
		std::string name = morphCtrl->m_morph->m_name;
		morphCtrlMap.emplace(std::make_pair(name, std::move(morphCtrl)));
	}
	m_morphControllers.clear();
	for (const auto& [m_blendShapeName, m_frame, m_weight] : vmd.m_morphs) {
		std::string morphName = UnicodeUtil::SjisToUtf8(m_blendShapeName);
		auto findIt = morphCtrlMap.find(morphName);
		VMDMorphController* morphCtrl = nullptr;
		if (findIt == std::end(morphCtrlMap)) {
			auto it = std::ranges::find(
				m_model->m_morphs, std::string_view{ morphName },
				[](const std::unique_ptr<MMDMorph>& mmdMorph) -> std::string_view { return mmdMorph->m_name; }
			);
			auto* mmdMorph = it == m_model->m_morphs.end() ? nullptr : it->get();
			if (mmdMorph != nullptr) {
				auto val = std::make_pair(
					morphName,
					std::make_unique<VMDMorphController>()
				);
				morphCtrl = val.second.get();
				morphCtrl->m_morph = mmdMorph;
				morphCtrlMap.emplace(std::move(val));
			}
		} else
			morphCtrl = findIt->second.get();
		if (morphCtrl != nullptr) {
			VMDMorphAnimationKey key{};
			key.m_time = static_cast<int32_t>(m_frame);
			key.m_weight = m_weight;
			morphCtrl->m_keys.push_back(key);
		}
	}
	m_morphControllers.reserve(morphCtrlMap.size());
	for (auto& val : morphCtrlMap | std::views::values) {
		std::ranges::sort(val->m_keys, {}, &VMDMorphAnimationKey::m_time);
		m_morphControllers.emplace_back(std::move(val));
	}
	morphCtrlMap.clear();
	m_maxKeyTime = CalculateMaxKeyTime();
	return true;
}

void VMDAnimation::Destroy() {
	m_model.reset();
	m_nodeControllers.clear();
	m_ikControllers.clear();
	m_morphControllers.clear();
	m_maxKeyTime = 0;
}

void VMDAnimation::Evaluate(const float t, const float animWeight) const {
	for (auto& nodeCtrl: m_nodeControllers)
		nodeCtrl->Evaluate(t, animWeight);
	for (auto& ikCtrl: m_ikControllers)
		ikCtrl->Evaluate(t, animWeight);
	for (auto& morphCtrl: m_morphControllers)
		morphCtrl->Evaluate(t, animWeight);
}

void VMDAnimation::SyncPhysics(const float t, const int frameCount) const {
	m_model->SaveBaseAnimation();
	for (int i = 0; i < frameCount; i++) {
		m_model->BeginAnimation();
		Evaluate(t, static_cast<float>(1 + i) / static_cast<float>(frameCount));
		m_model->UpdateMorphAnimation();
		m_model->UpdateNodeAnimation(false);
		m_model->UpdatePhysicsAnimation(1.0f / 30.0f);
		m_model->UpdateNodeAnimation(true);
	}
}

int32_t VMDAnimation::CalculateMaxKeyTime() const {
	int32_t maxTime = 0;
	for (const auto& nodeController : m_nodeControllers) {
		const auto& keys = nodeController->m_keys;
		if (!keys.empty())
			maxTime = (std::max)(maxTime, keys.rbegin()->m_time);
	}
	for (const auto& ikController : m_ikControllers) {
		const auto& keys = ikController->m_keys;
		if (!keys.empty())
			maxTime = (std::max)(maxTime, keys.rbegin()->m_time);
	}
	for (const auto& morphController : m_morphControllers) {
		const auto& keys = morphController->m_keys;
		if (!keys.empty())
			maxTime = (std::max)(maxTime, keys.rbegin()->m_time);
	}
	return maxTime;
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

VMDIKController::VMDIKController()
	: m_ikSolver(nullptr)
	, m_startKeyIndex(0) {
}

void VMDIKController::Evaluate(const float t, const float animWeight) {
	if (m_ikSolver == nullptr)
		return;
	if (m_keys.empty()) {
		m_ikSolver->m_enable = true;
		return;
	}
	const auto boundIt = FindBoundKey(m_keys, static_cast<int32_t>(t), m_startKeyIndex);
	bool enable;
	if (boundIt == std::end(m_keys))
		enable = m_keys.rbegin()->m_enable;
	else {
		enable = m_keys.begin()->m_enable;
		if (boundIt != std::begin(m_keys)) {
			const auto& [m_time, m_enable] = *(boundIt - 1);
			enable = m_enable;
			m_startKeyIndex = std::distance(m_keys.cbegin(), boundIt);
		}
	}
	if (animWeight != 1.0f && animWeight < 1.0f)
		m_ikSolver->m_enable = m_ikSolver->m_baseAnimEnable;
	else
		m_ikSolver->m_enable = enable;
}

VMDMorphController::VMDMorphController()
	: m_morph(nullptr)
	, m_startKeyIndex(0) {
}

void VMDMorphController::Evaluate(const float t, const float animWeight) {
	if (m_morph == nullptr)
		return;
	if (m_keys.empty())
		return;
	float weight;
	const auto boundIt = FindBoundKey(m_keys, static_cast<int32_t>(t), m_startKeyIndex);
	if (boundIt == std::end(m_keys))
		weight = m_keys.rbegin()->m_weight;
	else {
		weight = boundIt->m_weight;
		if (boundIt != std::begin(m_keys)) {
			auto [m_time0, m_weight0] = *(boundIt - 1);
			auto [m_time1, m_weight1] = *boundIt;
			const auto timeRange = static_cast<float>(m_time1 - m_time0);
			const float time = (t - static_cast<float>(m_time0)) / timeRange;
			weight = (m_weight1 - m_weight0) * time + m_weight0;
			m_startKeyIndex = std::distance(m_keys.cbegin(), boundIt);
		}
	}
	if (animWeight == 1.0f)
		m_morph->m_weight = weight;
	else
		m_morph->m_weight = glm::mix(m_morph->m_saveAnimWeight, weight, animWeight);
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

VMDCameraController::VMDCameraController()
	: m_startKeyIndex(0) {
}

void VMDCameraController::Evaluate(const float t) {
	if (m_keys.empty())
		return;
	const auto boundIt = FindBoundKey(m_keys, static_cast<int32_t>(t), m_startKeyIndex);
	if (boundIt == std::end(m_keys)) {
		const auto& selectKey = m_keys[m_keys.size() - 1];
		m_camera.m_interest = selectKey.m_interest;
		m_camera.m_rotate = selectKey.m_rotate;
		m_camera.m_distance = selectKey.m_distance;
		m_camera.m_fov = selectKey.m_fov;
	} else {
		const auto& selectKey = *boundIt;
		m_camera.m_interest = selectKey.m_interest;
		m_camera.m_rotate = selectKey.m_rotate;
		m_camera.m_distance = selectKey.m_distance;
		m_camera.m_fov = selectKey.m_fov;
		if (boundIt != std::begin(m_keys)) {
			const auto& key = *(boundIt - 1);
			const auto& [m_time, m_interest, m_rotate, m_distance, m_fov
						, m_ixBezier, m_iyBezier, m_izBezier, m_rotateBezier, m_distanceBezier, m_fovBezier]
					= *boundIt;
			if ((m_time - key.m_time) > 1) {
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
			} else {
				m_camera.m_interest = key.m_interest;
				m_camera.m_rotate = key.m_rotate;
				m_camera.m_distance = key.m_distance;
				m_camera.m_fov = key.m_fov;
			}
			m_startKeyIndex = std::distance(m_keys.cbegin(), boundIt);
		}
	}
}

void VMDCameraController::AddKey(const VMDCameraAnimationKey& key) {
	m_keys.push_back(key);
}

void VMDCameraController::SortKeys() {
	std::ranges::sort(m_keys, {}, &VMDCameraAnimationKey::m_time);
}

VMDCameraAnimation::VMDCameraAnimation() {
	Destroy();
}

bool VMDCameraAnimation::Create(const VMDReader& vmd) {
	if (!vmd.m_cameras.empty()) {
		m_cameraController = std::make_unique<VMDCameraController>();
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
			m_cameraController->AddKey(key);
		}
		m_cameraController->SortKeys();
	} else
		return false;
	return true;
}

void VMDCameraAnimation::Destroy() {
	m_cameraController.reset();
}

void VMDCameraAnimation::Evaluate(const float t) {
	m_cameraController->Evaluate(t);
	m_camera = m_cameraController->m_camera;
}
