#include "VMDAnimation.h"

#include "../base/Util.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <ranges>
#include <glm/gtc/matrix_transform.hpp>

void SetVMDBezier(VMDBezier& bezier, const unsigned char* cp) {
	const int x0 = cp[0];
	const int y0 = cp[4];
	const int x1 = cp[8];
	const int y1 = cp[12];

	bezier.m_cp1 = glm::vec2(static_cast<float>(x0) / 127.0f, static_cast<float>(y0) / 127.0f);
	bezier.m_cp2 = glm::vec2(static_cast<float>(x1) / 127.0f, static_cast<float>(y1) / 127.0f);
}

namespace
{
	glm::mat3 InvZ(const glm::mat3& m) {
		const glm::mat3 invZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
		return invZ * m * invZ;
	}
}

float VMDBezier::EvalX(const float t) const {
	const float t2 = t * t;
	const float t3 = t2 * t;
	const float it = 1.0f - t;
	const float it2 = it * it;
	const float it3 = it2 * it;
	const float x[4] = {
		0,
		m_cp1.x,
		m_cp2.x,
		1,
	};

	return t3 * x[3] + 3 * t2 * it * x[2] + 3 * t * it2 * x[1] + it3 * x[0];
}

float VMDBezier::EvalY(const float t) const {
	const float t2 = t * t;
	const float t3 = t2 * t;
	const float it = 1.0f - t;
	const float it2 = it * it;
	const float it3 = it2 * it;
	const float y[4] = {
		0,
		m_cp1.y,
		m_cp2.y,
		1,
	};

	return t3 * y[3] + 3 * t2 * it * y[2] + 3 * t * it2 * y[1] + it3 * y[0];
}

glm::vec2 VMDBezier::Eval(const float t) const {
	return {EvalX(t), EvalY(t)};
}

float VMDBezier::FindBezierX(const float time) const {
	constexpr float e = 0.00001f;
	float start = 0.0f;
	float stop = 1.0f;
	float t = 0.5f;
	float x = EvalX(t);
	while (std::abs(time - x) > e) {
		if (time < x)
			stop = t;
		else
			start = t;
		t = (stop + start) * 0.5f;
		x = EvalX(t);
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
			const auto &key = *(boundIt - 1);
			const auto& [m_time
						, m_translate
						, m_rotate
						, m_txBezier
						, m_tyBezier
						, m_tzBezier
						, m_rotBezier]
			= *boundIt;

			const auto timeRange = static_cast<float>(m_time - key.m_time);
			const float time = (t - static_cast<float>(key.m_time)) / timeRange;
			const float tx_x = m_txBezier.FindBezierX(time);
			const float ty_x = m_tyBezier.FindBezierX(time);
			const float tz_x = m_tzBezier.FindBezierX(time);
			const float rot_x = m_rotBezier.FindBezierX(time);
			const float tx_y = m_txBezier.EvalY(tx_x);
			const float ty_y = m_tyBezier.EvalY(ty_x);
			const float tz_y = m_tzBezier.EvalY(tz_x);
			const float rot_y = m_rotBezier.EvalY(rot_x);

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

void VMDNodeController::SortKeys() {
	std::ranges::sort(m_keys,
		[](const VMDNodeAnimationKey &a, const VMDNodeAnimationKey &b)
		{ return a.m_time < b.m_time; }
	);
}

VMDAnimation::VMDAnimation()
	: m_maxKeyTime(0) {
}

bool VMDAnimation::Add(const VMDFile & vmd) {
	// Node Controller
	std::map<std::string, std::unique_ptr<VMDNodeController> > nodeCtrlMap;
	for (auto &nodeCtrl: m_nodeControllers) {
		std::string name = nodeCtrl->m_node->m_name;
		nodeCtrlMap.emplace(std::make_pair(name, std::move(nodeCtrl)));
	}
	m_nodeControllers.clear();
	for (const auto &motion: vmd.m_motions) {
		std::string nodeName;
		UnicodeUtil::ConvU16ToU8(UnicodeUtil::ConvertSjisToU16String(motion.m_boneName), nodeName);
		auto findIt = nodeCtrlMap.find(nodeName);
		VMDNodeController *nodeCtrl = nullptr;
		if (findIt == std::end(nodeCtrlMap)) {
			auto it = std::ranges::find(
					m_model->m_nodes, std::string_view{nodeName},
					[](const std::unique_ptr<MMDNode>& node) -> std::string_view
					{ return node->m_name; }
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
	for (auto &val: nodeCtrlMap | std::views::values) {
		val->SortKeys();
		m_nodeControllers.emplace_back(std::move(val));
	}
	nodeCtrlMap.clear();

	// IK Controller
	std::map<std::string, std::unique_ptr<VMDIKController> > ikCtrlMap;
	for (auto &ikCtrl: m_ikControllers) {
		std::string name = ikCtrl->m_ikSolver->m_ikNode->m_name;
		ikCtrlMap.emplace(std::make_pair(name, std::move(ikCtrl)));
	}
	m_ikControllers.clear();
	for (const auto& ik: vmd.m_iks) {
		for (const auto& [m_name, m_enable]: ik.m_ikInfos) {
			std::string ikName;
			UnicodeUtil::ConvU16ToU8(UnicodeUtil::ConvertSjisToU16String(m_name), ikName);
			auto findIt = ikCtrlMap.find(ikName);
			VMDIKController *ikCtrl = nullptr;
			if (findIt == std::end(ikCtrlMap)) {
				auto it = std::ranges::find(
					m_model->m_ikSolvers, std::string_view{ikName},
					[](const std::unique_ptr<MMDIkSolver>& ikSolver) -> std::string_view
					{ return ikSolver->m_ikNode->m_name; }
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
	for (auto &val: ikCtrlMap | std::views::values) {
		val->SortKeys();
		m_ikControllers.emplace_back(std::move(val));
	}
	ikCtrlMap.clear();

	// Morph Controller
	std::map<std::string, std::unique_ptr<VMDMorphController> > morphCtrlMap;
	for (auto &morphCtrl: m_morphControllers) {
		std::string name = morphCtrl->m_morph->m_name;
		morphCtrlMap.emplace(std::make_pair(name, std::move(morphCtrl)));
	}
	m_morphControllers.clear();
	for (const auto &[m_blendShapeName, m_frame, m_weight]: vmd.m_morphs) {
		std::string morphName;
		UnicodeUtil::ConvU16ToU8(UnicodeUtil::ConvertSjisToU16String(m_blendShapeName), morphName);
		auto findIt = morphCtrlMap.find(morphName);
		VMDMorphController *morphCtrl = nullptr;
		if (findIt == std::end(morphCtrlMap)) {
			auto it = std::ranges::find(
					m_model->m_morphs, std::string_view{morphName},
					[](const std::unique_ptr<MMDMorph>& mmdMorph) -> std::string_view
					{ return mmdMorph->m_name; }
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
	for (auto &val: morphCtrlMap | std::views::values) {
		val->SortKeys();
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
	for (auto &nodeCtrl: m_nodeControllers)
		nodeCtrl->Evaluate(t, animWeight);

	for (auto &ikCtrl: m_ikControllers)
		ikCtrl->Evaluate(t, animWeight);

	for (auto &morphCtrl: m_morphControllers)
		morphCtrl->Evaluate(t, animWeight);
}

void VMDAnimation::SyncPhysics(const float t, const int frameCount) const {
	/*
	すぐにアニメーションを反映すると、Physics が破たんする場合がある。
	例：足がスカートを突き破る等
	アニメーションを反映する際、初期状態から数フレームかけて、
	目的のポーズへ遷移させる。
	*/
	m_model->SaveBaseAnimation();

	// Physicsを反映する
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
	for (const auto &nodeController: m_nodeControllers) {
		const auto &keys = nodeController->m_keys;
		if (!keys.empty())
			maxTime = (std::max)(maxTime, keys.rbegin()->m_time);
	}

	for (const auto &ikController: m_ikControllers) {
		const auto &keys = ikController->m_keys;
		if (!keys.empty())
			maxTime = (std::max)(maxTime, keys.rbegin()->m_time);
	}

	for (const auto &morphController: m_morphControllers) {
		const auto &keys = morphController->m_keys;
		if (!keys.empty())
			maxTime = (std::max)(maxTime, keys.rbegin()->m_time);
	}

	return maxTime;
}

void VMDNodeAnimationKey::Set(const VMDMotion & motion) {
	m_time = static_cast<int32_t>(motion.m_frame);

	m_translate = motion.m_translate * glm::vec3(1, 1, -1);

	const glm::quat q = motion.m_quaternion;
	const auto rot0 = glm::mat3_cast(q);
	const auto rot1 = InvZ(rot0);
	m_rotate = glm::quat_cast(rot1);

	SetVMDBezier(m_txBezier, &motion.m_interpolation[0]);
	SetVMDBezier(m_tyBezier, &motion.m_interpolation[1]);
	SetVMDBezier(m_tzBezier, &motion.m_interpolation[2]);
	SetVMDBezier(m_rotBezier, &motion.m_interpolation[3]);
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
			const auto &[m_time, m_enable] = *(boundIt - 1);
			enable = m_enable;

			m_startKeyIndex = std::distance(m_keys.cbegin(), boundIt);
		}
	}

	if (animWeight != 1.0f && animWeight < 1.0f)
		m_ikSolver->m_enable = m_ikSolver->m_baseAnimEnable;
	else
		m_ikSolver->m_enable = enable;
}

void VMDIKController::SortKeys() {
	std::ranges::sort(m_keys,
		[](const VMDIKAnimationKey &a, const VMDIKAnimationKey &b)
		{ return a.m_time < b.m_time; }
	);
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

void VMDMorphController::SortKeys() {
	std::ranges::sort(m_keys,
		[](const VMDMorphAnimationKey &a, const VMDMorphAnimationKey &b)
		{ return a.m_time < b.m_time; }
	);
}
