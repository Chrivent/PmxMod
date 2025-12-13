#include "MMDIkSolver.h"

#include <algorithm>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

MMDIkSolver::MMDIkSolver()
	: m_ikNode(nullptr)
	, m_ikTarget(nullptr)
	, m_iterateCount(1)
	, m_limitAngle(glm::pi<float>() * 2.0f)
	, m_enable(true)
	, m_baseAnimEnable(true) {
}

void MMDIkSolver::Solve() {
	if (!m_enable)
		return;
	for (auto &chain: m_chains) {
		chain.m_prevAngle = glm::vec3(0);
		chain.m_node->m_ikRotate = glm::quat(1, 0, 0, 0);
		chain.m_planeModeAngle = 0;
		chain.m_node->UpdateLocalTransform();
		chain.m_node->UpdateGlobalTransform();
	}
	float maxDist = std::numeric_limits<float>::max();
	for (uint32_t i = 0; i < m_iterateCount; i++) {
		SolveCore(i);
		auto targetPos = glm::vec3(m_ikTarget->m_global[3]);
		auto ikPos = glm::vec3(m_ikNode->m_global[3]);
		const float dist = glm::length(targetPos - ikPos);
		if (dist < maxDist) {
			maxDist = dist;
			for (auto &chain: m_chains)
				chain.m_saveIKRot = chain.m_node->m_ikRotate;
		} else {
			for (const auto &chain: m_chains) {
				chain.m_node->m_ikRotate = chain.m_saveIKRot;
				chain.m_node->UpdateLocalTransform();
				chain.m_node->UpdateGlobalTransform();
			}
			break;
		}
	}
}

float MMDIkSolver::NormalizeAngle(const float angle) {
	float ret = angle;
	while (ret >= glm::two_pi<float>())
		ret -= glm::two_pi<float>();
	while (ret < 0)
		ret += glm::two_pi<float>();
	return ret;
}

float MMDIkSolver::DiffAngle(const float a, const float b) {
	const float diff = NormalizeAngle(a) - NormalizeAngle(b);
	if (diff > glm::pi<float>())
		return diff - glm::two_pi<float>();
	if (diff < -glm::pi<float>())
		return diff + glm::two_pi<float>();
	return diff;
}

glm::vec3 MMDIkSolver::Decompose(const glm::mat3& m, const glm::vec3& before) {
	glm::vec3 r;
	const float sy = -m[0][2];
	constexpr float e = 1.0e-6f;
	if (1.0f - std::abs(sy) < e) {
		r.y = std::asin(sy);
		const float sx = std::sin(before.x);
		const float sz = std::sin(before.z);
		if (std::abs(sx) < std::abs(sz)) {
			const float cx = std::cos(before.x);
			if (cx > 0) {
				r.x = 0;
				r.z = std::asin(-m[1][0]);
			} else {
				r.x = glm::pi<float>();
				r.z = std::asin(m[1][0]);
			}
		} else {
			const float cz = std::cos(before.z);
			if (cz > 0) {
				r.z = 0;
				r.x = std::asin(-m[2][1]);
			} else {
				r.z = glm::pi<float>();
				r.x = std::asin(m[2][1]);
			}
		}
	} else {
		r.x = std::atan2(m[1][2], m[2][2]);
		r.y = std::asin(-m[0][2]);
		r.z = std::atan2(m[0][1], m[0][0]);
	}
	constexpr auto pi = glm::pi<float>();
	glm::vec3 tests[] =
	{
		{r.x + pi, pi - r.y, r.z + pi},
		{r.x + pi, pi - r.y, r.z - pi},
		{r.x + pi, -pi - r.y, r.z + pi},
		{r.x + pi, -pi - r.y, r.z - pi},
		{r.x - pi, pi - r.y, r.z + pi},
		{r.x - pi, pi - r.y, r.z - pi},
		{r.x - pi, -pi - r.y, r.z + pi},
		{r.x - pi, -pi - r.y, r.z - pi},
	};
	const float errX = std::abs(DiffAngle(r.x, before.x));
	const float errY = std::abs(DiffAngle(r.y, before.y));
	const float errZ = std::abs(DiffAngle(r.z, before.z));
	float minErr = errX + errY + errZ;
	for (const auto test: tests) {
		const float err = std::abs(DiffAngle(test.x, before.x))
						+ std::abs(DiffAngle(test.y, before.y))
						+ std::abs(DiffAngle(test.z, before.z));
		if (err < minErr) {
			minErr = err;
			r = test;
		}
	}
	return r;
}

void MMDIkSolver::SolveCore(uint32_t iteration) {
	auto ikPos = glm::vec3(m_ikNode->m_global[3]);
	for (size_t chainIdx = 0; chainIdx < m_chains.size(); chainIdx++) {
		auto &chain = m_chains[chainIdx];
		MMDNode *chainNode = chain.m_node;
		if (chainNode == m_ikTarget)
			continue;
		if (chain.m_enableAxisLimit) {
			if ((chain.m_limitMin.x != 0 || chain.m_limitMax.x != 0) &&
			    (chain.m_limitMin.y == 0 || chain.m_limitMax.y == 0) &&
			    (chain.m_limitMin.z == 0 || chain.m_limitMax.z == 0)) {
				SolvePlane(iteration, chainIdx, 0);
				continue;
			}
			if ((chain.m_limitMin.y != 0 || chain.m_limitMax.y != 0) &&
			    (chain.m_limitMin.x == 0 || chain.m_limitMax.x == 0) &&
			    (chain.m_limitMin.z == 0 || chain.m_limitMax.z == 0)) {
				SolvePlane(iteration, chainIdx, 1);
				continue;
			}
			if ((chain.m_limitMin.z != 0 || chain.m_limitMax.z != 0) &&
			    (chain.m_limitMin.x == 0 || chain.m_limitMax.x == 0) &&
			    (chain.m_limitMin.y == 0 || chain.m_limitMax.y == 0)) {
				SolvePlane(iteration, chainIdx, 2);
				continue;
			}
		}
		auto targetPos = glm::vec3(m_ikTarget->m_global[3]);
		auto invChain = glm::inverse(chain.m_node->m_global);
		auto chainIkPos = glm::vec3(invChain * glm::vec4(ikPos, 1));
		auto chainTargetPos = glm::vec3(invChain * glm::vec4(targetPos, 1));
		auto chainIkVec = glm::normalize(chainIkPos);
		auto chainTargetVec = glm::normalize(chainTargetPos);
		auto dot = glm::dot(chainTargetVec, chainIkVec);
		dot = glm::clamp(dot, -1.0f, 1.0f);
		float angle = std::acos(dot);
		float angleDeg = glm::degrees(angle);
		if (angleDeg < 1.0e-3f)
			continue;
		angle = glm::clamp(angle, -m_limitAngle, m_limitAngle);
		auto cross = glm::normalize(glm::cross(chainTargetVec, chainIkVec));
		auto rot = glm::rotate(glm::quat(1, 0, 0, 0), angle, cross);
		auto animRot = chainNode->m_animRotate * chainNode->m_rotate;
		auto chainRot = chainNode->m_ikRotate * animRot * rot;
		if (chain.m_enableAxisLimit) {
			auto chainRotM = glm::mat3_cast(chainRot);
			auto rotXYZ = Decompose(chainRotM, chain.m_prevAngle);
			glm::vec3 clampXYZ;
			clampXYZ = glm::clamp(rotXYZ, chain.m_limitMin, chain.m_limitMax);
			clampXYZ = glm::clamp(clampXYZ - chain.m_prevAngle, -m_limitAngle, m_limitAngle) + chain.m_prevAngle;
			auto r = glm::rotate(glm::quat(1, 0, 0, 0), clampXYZ.x, glm::vec3(1, 0, 0));
			r = glm::rotate(r, clampXYZ.y, glm::vec3(0, 1, 0));
			r = glm::rotate(r, clampXYZ.z, glm::vec3(0, 0, 1));
			chainRotM = glm::mat3_cast(r);
			chain.m_prevAngle = clampXYZ;
			chainRot = glm::quat_cast(chainRotM);
		}
		auto ikRot = chainRot * glm::inverse(animRot);
		chainNode->m_ikRotate = ikRot;
		chainNode->UpdateLocalTransform();
		chainNode->UpdateGlobalTransform();
	}
}

void MMDIkSolver::SolvePlane(uint32_t iteration, size_t chainIdx, int RotateAxisIndex) {
	auto RotateAxis = glm::vec3(1, 0, 0);
	if (RotateAxisIndex == 1)
		RotateAxis = glm::vec3(0, 1, 0);
	else if (RotateAxisIndex == 2)
		RotateAxis = glm::vec3(0, 0, 1);
	auto &chain = m_chains[chainIdx];
	auto ikPos = glm::vec3(m_ikNode->m_global[3]);
	auto targetPos = glm::vec3(m_ikTarget->m_global[3]);
	auto invChain = glm::inverse(chain.m_node->m_global);
	auto chainIkPos = glm::vec3(invChain * glm::vec4(ikPos, 1));
	auto chainTargetPos = glm::vec3(invChain * glm::vec4(targetPos, 1));
	auto chainIkVec = glm::normalize(chainIkPos);
	auto chainTargetVec = glm::normalize(chainTargetPos);
	auto dot = glm::dot(chainTargetVec, chainIkVec);
	dot = glm::clamp(dot, -1.0f, 1.0f);
	float angle = std::acos(dot);
	angle = glm::clamp(angle, -m_limitAngle, m_limitAngle);
	auto rot1 = glm::rotate(glm::quat(1, 0, 0, 0), angle, RotateAxis);
	auto targetVec1 = rot1 * chainTargetVec;
	auto dot1 = glm::dot(targetVec1, chainIkVec);
	auto rot2 = glm::rotate(glm::quat(1, 0, 0, 0), -angle, RotateAxis);
	auto targetVec2 = rot2 * chainTargetVec;
	auto dot2 = glm::dot(targetVec2, chainIkVec);
	auto newAngle = chain.m_planeModeAngle;
	if (dot1 > dot2)
		newAngle += angle;
	else
		newAngle -= angle;
	if (iteration == 0) {
		if (newAngle < chain.m_limitMin[RotateAxisIndex] || newAngle > chain.m_limitMax[RotateAxisIndex]) {
			if (-newAngle > chain.m_limitMin[RotateAxisIndex] && -newAngle < chain.m_limitMax[RotateAxisIndex])
				newAngle *= -1;
			else {
				auto halfRad = (chain.m_limitMin[RotateAxisIndex] + chain.m_limitMax[RotateAxisIndex]) * 0.5f;
				if (glm::abs(halfRad - newAngle) > glm::abs(halfRad + newAngle))
					newAngle *= -1;
			}
		}
	}
	newAngle = glm::clamp(newAngle, chain.m_limitMin[RotateAxisIndex], chain.m_limitMax[RotateAxisIndex]);
	chain.m_planeModeAngle = newAngle;
	auto ikRotM = glm::rotate(glm::quat(1, 0, 0, 0), newAngle, RotateAxis)
				* glm::inverse(chain.m_node->m_animRotate * chain.m_node->m_rotate);
	chain.m_node->m_ikRotate = ikRotM;
	chain.m_node->UpdateLocalTransform();
	chain.m_node->UpdateGlobalTransform();
}

MMDIkSolver* MMDIKManager::GetIKSolver(const std::string& ikName) {
	const auto findIt = std::ranges::find_if(m_ikSolvers,
		[&ikName](const std::unique_ptr<MMDIkSolver> &ikSolver)
		{ return ikSolver->m_ikNode->m_name == ikName; }
	);
	if (findIt == m_ikSolvers.end())
		return nullptr;
	return m_ikSolvers[findIt - m_ikSolvers.begin()].get();
}
