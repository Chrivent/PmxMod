#include "ModelPose.h"

#include <ranges>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

namespace Chrivent {
	void ModelPose::SetupParallelUpdate() const {
		if (!model.parallelUpdateCount)
			model.parallelUpdateCount = (std::max)(1u, std::thread::hardware_concurrency());
		model.parallelUpdateCount = std::min<size_t>(model.parallelUpdateCount, 16);
		model.updateRanges.resize(model.parallelUpdateCount);
		model.parallelUpdateFutures.resize(model.parallelUpdateCount - 1);
		const size_t totalVertexCount = model.positions.size();
		constexpr size_t lowerVertexCount = 1000;
		if (totalVertexCount < model.updateRanges.size() * lowerVertexCount) {
			const size_t numRanges = (totalVertexCount + lowerVertexCount - 1) / lowerVertexCount;
			for (size_t i = 0; i < model.updateRanges.size(); i++) {
				auto& [vertexOffset, vertexCount] = model.updateRanges[i];
				if (i < numRanges) {
					vertexOffset = i * lowerVertexCount;
					vertexCount  = (std::min)(lowerVertexCount, totalVertexCount - vertexOffset);
				} else {
					vertexOffset = 0;
					vertexCount = 0;
				}
			}
			return;
		}
		const size_t numVertexCount = totalVertexCount / model.updateRanges.size();
		size_t offset = 0;
		for (size_t i = 0; i < model.updateRanges.size(); i++) {
			auto& [vertexOffset, vertexCount] = model.updateRanges[i];
			vertexOffset = offset;
			vertexCount  = numVertexCount + (i == 0 ? totalVertexCount % model.updateRanges.size() : 0);
			offset += vertexCount;
		}
	}

	void ModelPose::UpdateNodeAnimation(const bool afterPhysicsAnim) const {
		const auto Pred = [&](const std::reference_wrapper<Node>& node) {
			return node.get().isDeformAfterPhysics == afterPhysicsAnim;
		};
		for (auto& nodeRef : model.sortedNodes | std::views::filter(Pred))
			nodeRef.get().UpdateLocalTransform();
		for (auto& nodeRef : model.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeRef.get();
			if (node.parent.expired())
				node.UpdateGlobalTransform();
		}
		for (auto& nodeRef : model.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeRef.get();
			if (!node.appendNode.expired()) {
				node.UpdateAppendTransform();
				node.UpdateGlobalTransform();
			}
			if (const auto ikSolver = node.ikSolver.lock()) {
				ikSolver->Solve();
				node.UpdateGlobalTransform();
			}
		}
	}

	void ModelPose::ResetPhysics() const {
		for (auto& rb : model.rigidBodies) {
			rb->ApplyActivation(false);
			rb->ResetTransform();
		}
		model.physics->world->stepSimulation(
			1.0f / 60.0f, model.physics->maxSubStepCount,
			static_cast<btScalar>(1.0f / model.physics->fps));
		for (auto& rb : model.rigidBodies) {
			rb->ReflectGlobalTransform();
			rb->CalcLocalTransform();
		}
		for (const auto& node : model.nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
		for (auto& rb : model.rigidBodies)
			rb->Reset(model.physics.get());
	}

	void ModelPose::UpdatePhysicsAnimation(const float elapsed) const {
		for (auto& rb : model.rigidBodies)
			rb->ApplyActivation(true);
		model.physics->world->stepSimulation(
			elapsed, model.physics->maxSubStepCount,
			static_cast<btScalar>(1.0f / model.physics->fps));
		for (auto& rb : model.rigidBodies) {
			rb->ReflectGlobalTransform();
			rb->CalcLocalTransform();
		}
		for (const auto& node : model.nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
	}

	void ModelPose::Update() const {
		for (size_t i = 0; i < model.nodes.size(); i++)
			model.transforms[i] = model.nodes[i]->global * model.nodes[i]->inverseInit;
		if (model.parallelUpdateCount != model.updateRanges.size())
			SetupParallelUpdate();
		const size_t futureCount = model.parallelUpdateFutures.size();
		for (size_t i = 0; i < futureCount; i++) {
			if (model.updateRanges[i + 1].vertexCount != 0) {
				model.parallelUpdateFutures[i] = std::async(std::launch::async,
				[this, range = model.updateRanges[i + 1]] { UpdateSkinning(range); });
			}
		}
		UpdateSkinning(model.updateRanges[0]);
		for (size_t i = 0; i < futureCount; i++) {
			if (model.updateRanges[i + 1].vertexCount != 0)
				model.parallelUpdateFutures[i].wait();
		}
	}

	void ModelPose::UpdateSkinning(const UpdateRange& range) const {
		for (size_t index = range.vertexOffset; index < range.vertexOffset + range.vertexCount; index++) {
			const auto& position = model.positions[index];
			const auto& normal = model.normals[index];
			const auto& uv = model.uvs[index];
			const auto& morphPos = model.morphPositions[index];
			const auto& morphUv = model.morphUVs[index];
			const auto& [weightType, boneIndices, boneWeights,
				sphericalDeformC, sphericalDeformR0,
				sphericalDeformR1] = model.vertexBoneInfos[index];
			auto& updatePos = model.updatePositions[index];
			auto& updateNormal = model.updateNormals[index];
			auto& updateUv = model.updateUVs[index];
			glm::mat4 m(1.0f);
			switch (weightType) {
				case WeightType::BoneDeform1: {
					m = model.transforms[boneIndices[0]];
					break;
				}
				case WeightType::BoneDeform2: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					m = model.transforms[i0] * w0 + model.transforms[i1] * w1;
					break;
				}
				case WeightType::BoneDeform4: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto i2 = boneIndices[2], i3 = boneIndices[3];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					const auto w2 = boneWeights[2], w3 = boneWeights[3];
					m = model.transforms[i0] * w0 + model.transforms[i1] * w1
						+ model.transforms[i2] * w2 + model.transforms[i3] * w3;
					break;
				}
				case WeightType::SphericalDeform: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = 1.0f - w0;
					const auto center = sphericalDeformC;
					const auto cr0 = sphericalDeformR0;
					const auto cr1 = sphericalDeformR1;
					const auto q0 = glm::quat_cast(model.nodes[i0]->global);
					const auto q1 = glm::quat_cast(model.nodes[i1]->global);
					const auto rotMat = glm::mat3_cast(glm::slerp(q0, q1, w1));
					const auto m0 = model.transforms[i0], m1 = model.transforms[i1];
					const auto pos = position + morphPos;
					updatePos = rotMat * (pos - center)
						+ glm::vec3(m0 * glm::vec4(cr0, 1.0f)) * w0
						+ glm::vec3(m1 * glm::vec4(cr1, 1.0f)) * w1;
					updateNormal = rotMat * normal;
					break;
				}
				case WeightType::QuaternionDeform: {
					glm::dualquat dq[4]{};
					float w[4]{};
					for (int bi = 0; bi < 4; bi++) {
						const auto boneId = boneIndices[bi];
						if (boneId == -1)
							continue;
						dq[bi] = glm::normalize(glm::dualquat_cast(
							glm::mat3x4(glm::transpose(model.transforms[boneId]))
						));
						w[bi] = boneWeights[bi];
					}
					if (glm::dot(dq[0].real, dq[1].real) < 0.0f)
						w[1] *= -1.0f;
					if (glm::dot(dq[0].real, dq[2].real) < 0.0f)
						w[2] *= -1.0f;
					if (glm::dot(dq[0].real, dq[3].real) < 0.0f)
						w[3] *= -1.0f;
					const auto blendDq = glm::normalize(
						w[0] * dq[0] + w[1] * dq[1]
						+ w[2] * dq[2] + w[3] * dq[3]
					);
					m = glm::transpose(glm::mat3x4_cast(blendDq));
					break;
				}
			}
			if (weightType != WeightType::SphericalDeform) {
				updatePos = glm::vec3(m * glm::vec4(position + morphPos, 1.0f));
				updateNormal = glm::normalize(glm::mat3(m) * normal);
			}
			updateUv = uv + glm::vec2(morphUv.x, morphUv.y);
		}
	}
}
