#include "IkSolver.h"

#include "Node.h"

#include <cmath>
#include <limits>

namespace Chrivent {
	void IkSolver::SolveCore(uint32_t iteration) {
		auto ikNodePtr = info.ikNode.lock();
		auto ikTargetPtr = info.ikTarget.lock();
		if (!ikNodePtr || !ikTargetPtr)
			return;
		auto ikPos = glm::vec3(ikNodePtr->GetInfo().global[3]);
		for (size_t chainIdx = 0; chainIdx < info.chains.size(); chainIdx++) {
			auto &chain = info.chains[chainIdx];
			auto chainNodePtr = chain.node.lock();
			if (!chainNodePtr || chainNodePtr == ikTargetPtr)
				continue;
			if (chain.enableAxisLimit) {
				if ((chain.limitMin.x != 0 || chain.limitMax.x != 0) &&
					(chain.limitMin.y == 0 || chain.limitMax.y == 0) &&
					(chain.limitMin.z == 0 || chain.limitMax.z == 0)) {
					SolvePlane(iteration, chainIdx, 0);
					continue;
				}
				if ((chain.limitMin.y != 0 || chain.limitMax.y != 0) &&
					(chain.limitMin.x == 0 || chain.limitMax.x == 0) &&
					(chain.limitMin.z == 0 || chain.limitMax.z == 0)) {
					SolvePlane(iteration, chainIdx, 1);
					continue;
				}
				if ((chain.limitMin.z != 0 || chain.limitMax.z != 0) &&
					(chain.limitMin.x == 0 || chain.limitMax.x == 0) &&
					(chain.limitMin.y == 0 || chain.limitMax.y == 0)) {
					SolvePlane(iteration, chainIdx, 2);
					continue;
				}
			}
			auto targetPos = glm::vec3(ikTargetPtr->GetInfo().global[3]);
			auto invChain = glm::inverse(chainNodePtr->GetInfo().global);
			auto chainIkPos = glm::vec3(invChain * glm::vec4(ikPos, 1));
			auto chainTargetPos = glm::vec3(invChain * glm::vec4(targetPos, 1));
			auto chainIkVec = glm::normalize(chainIkPos);
			auto chainTargetVec = glm::normalize(chainTargetPos);
			auto dot = glm::dot(chainTargetVec, chainIkVec);
			dot = glm::clamp(dot, -1.0f, 1.0f);
			float angle = std::acos(dot);
			if (angle < std::numeric_limits<float>::epsilon())
				continue;
			angle = glm::clamp(angle, -info.limitAngle, info.limitAngle);
			auto cross = glm::normalize(glm::cross(chainTargetVec, chainIkVec));
			auto rot = glm::rotate(glm::quat(1, 0, 0, 0), angle, cross);
			auto animRot = chainNodePtr->GetInfo().animRotate * chainNodePtr->GetInfo().rotate;
			auto chainRot = chainNodePtr->GetInfo().ikRotate * animRot * rot;
			if (chain.enableAxisLimit) {
				auto chainRotM = glm::mat3_cast(chainRot);
				auto currentEuler = Decompose(chainRotM, chain.prevAngle);
				glm::vec3 limitedEuler;
				limitedEuler = glm::clamp(currentEuler, chain.limitMin, chain.limitMax);
				limitedEuler = glm::clamp(limitedEuler - chain.prevAngle, -info.limitAngle, info.limitAngle) + chain.prevAngle;
				auto r = glm::rotate(glm::quat(1, 0, 0, 0), limitedEuler.x, glm::vec3(1, 0, 0));
				r = glm::rotate(r, limitedEuler.y, glm::vec3(0, 1, 0));
				r = glm::rotate(r, limitedEuler.z, glm::vec3(0, 0, 1));
				chainRotM = glm::mat3_cast(r);
				chain.prevAngle = limitedEuler;
				chainRot = glm::quat_cast(chainRotM);
			}
			auto ikRot = chainRot * glm::inverse(animRot);
			chainNodePtr->GetInfo().ikRotate = ikRot;
			chainNodePtr->UpdateLocalTransform();
			chainNodePtr->UpdateGlobalTransform();
		}
	}

	void IkSolver::SolvePlane(uint32_t iteration, size_t chainIdx, int rotateAxisIndex) {
		constexpr glm::vec3 axis[3] = {
			{ 1, 0, 0 },
			{ 0, 1, 0 },
			{ 0, 0, 1 }
		};
		const glm::vec3& rotateAxis = axis[rotateAxisIndex];
		auto &chain = info.chains[chainIdx];
		auto ikNodePtr = info.ikNode.lock();
		auto ikTargetPtr = info.ikTarget.lock();
		auto chainNodePtr = chain.node.lock();
		if (!ikNodePtr || !ikTargetPtr || !chainNodePtr)
			return;
		auto ikPos = glm::vec3(ikNodePtr->GetInfo().global[3]);
		auto targetPos = glm::vec3(ikTargetPtr->GetInfo().global[3]);
		auto invChain = glm::inverse(chainNodePtr->GetInfo().global);
		auto chainIkPos = glm::vec3(invChain * glm::vec4(ikPos, 1));
		auto chainTargetPos = glm::vec3(invChain * glm::vec4(targetPos, 1));
		auto chainIkVec = glm::normalize(chainIkPos);
		auto chainTargetVec = glm::normalize(chainTargetPos);
		auto dot = glm::dot(chainTargetVec, chainIkVec);
		dot = glm::clamp(dot, -1.0f, 1.0f);
		float angle = std::acos(dot);
		angle = glm::clamp(angle, -info.limitAngle, info.limitAngle);
		auto rot1 = glm::rotate(glm::quat(1, 0, 0, 0), angle, rotateAxis);
		auto targetVec1 = rot1 * chainTargetVec;
		auto dot1 = glm::dot(targetVec1, chainIkVec);
		auto rot2 = glm::rotate(glm::quat(1, 0, 0, 0), -angle, rotateAxis);
		auto targetVec2 = rot2 * chainTargetVec;
		auto dot2 = glm::dot(targetVec2, chainIkVec);
		auto newAngle = chain.planeModeAngle;
		auto sign = dot1 > dot2 ? 1.0f : -1.0f;
		newAngle += sign * angle;
		if (iteration == 0) {
			if (newAngle < chain.limitMin[rotateAxisIndex] || newAngle > chain.limitMax[rotateAxisIndex]) {
				if (-newAngle > chain.limitMin[rotateAxisIndex] && -newAngle < chain.limitMax[rotateAxisIndex])
					newAngle *= -1;
				else {
					auto halfRad = (chain.limitMin[rotateAxisIndex] + chain.limitMax[rotateAxisIndex]) * 0.5f;
					if (glm::abs(halfRad - newAngle) > glm::abs(halfRad + newAngle))
						newAngle *= -1;
				}
			}
		}
		newAngle = glm::clamp(newAngle, chain.limitMin[rotateAxisIndex], chain.limitMax[rotateAxisIndex]);
		chain.planeModeAngle = newAngle;
		auto ikRotM = glm::rotate(glm::quat(1, 0, 0, 0), newAngle, rotateAxis) *
			glm::inverse(chainNodePtr->GetInfo().animRotate * chainNodePtr->GetInfo().rotate);
		chainNodePtr->GetInfo().ikRotate = ikRotM;
		chainNodePtr->UpdateLocalTransform();
		chainNodePtr->UpdateGlobalTransform();
	}

	float IkSolver::NormalizeAngle(float angle) {
		angle = std::fmod(angle, glm::two_pi<float>());
		if (angle < 0)
			angle += glm::two_pi<float>();
		return angle;
	}
	
	float IkSolver::DiffAngle(const float a, const float b) {
		const float diff = NormalizeAngle(a) - NormalizeAngle(b);
		if (diff > glm::pi<float>())
			return diff - glm::two_pi<float>();
		if (diff < -glm::pi<float>())
			return diff + glm::two_pi<float>();
		return diff;
	}

	glm::vec3 IkSolver::Decompose(const glm::mat3& m, const glm::vec3& before) {
		glm::vec3 r;
		const float sy = -m[0][2];
		if (1.0f - std::abs(sy) < std::numeric_limits<float>::epsilon()) {
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
		glm::vec3 tests[] = {
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

	void IkSolver::Solve() {
		if (!info.enable)
			return;
		const auto ikNodePtr = info.ikNode.lock();
		const auto ikTargetPtr = info.ikTarget.lock();
		if (!ikNodePtr || !ikTargetPtr)
			return;
		for (auto &chain: info.chains) {
			const auto chainNodePtr = chain.node.lock();
			if (!chainNodePtr)
				continue;
			chain.prevAngle = glm::vec3(0);
			chainNodePtr->GetInfo().ikRotate = glm::quat(1, 0, 0, 0);
			chain.planeModeAngle = 0;
			chainNodePtr->UpdateLocalTransform();
			chainNodePtr->UpdateGlobalTransform();
		}
		float maxDist = std::numeric_limits<float>::max();
		for (uint32_t i = 0; i < info.iterateCount; i++) {
			SolveCore(i);
			auto targetPos = glm::vec3(ikTargetPtr->GetInfo().global[3]);
			auto ikPos = glm::vec3(ikNodePtr->GetInfo().global[3]);
			const float dist = glm::length(targetPos - ikPos);
			if (dist < maxDist) {
				maxDist = dist;
				for (auto &chain: info.chains) {
					if (const auto chainNodePtr = chain.node.lock())
						chain.saveIkRot = chainNodePtr->GetInfo().ikRotate;
				}
			} else {
				for (const auto &chain: info.chains) {
					if (const auto chainNodePtr = chain.node.lock()) {
						chainNodePtr->GetInfo().ikRotate = chain.saveIkRot;
						chainNodePtr->UpdateLocalTransform();
						chainNodePtr->UpdateGlobalTransform();
					}
				}
				break;
			}
		}
	}
}
