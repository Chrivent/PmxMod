#include "ModelPose.h"

#include <ranges>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

namespace Chrivent {
	void ModelPose::UpdateSkinning(const UpdateRange& range) const {
		for (size_t index = range.vertexOffset; index < range.vertexOffset + range.vertexCount; index++) {
			const auto& position = model.geometry.positions[index];
			const auto& normal = model.geometry.normals[index];
			const auto& uv = model.geometry.uvs[index];
			const auto& morphPos = model.morphData.morphPositions[index];
			const auto& morphUv = model.morphData.morphUVs[index];
			const auto& [weightType, boneIndices, boneWeights,
				sphericalDeformC, sphericalDeformR0,
				sphericalDeformR1] = model.geometry.vertexBoneInfos[index];
			auto& updatePos = model.geometry.updatePositions[index];
			auto& updateNormal = model.geometry.updateNormals[index];
			auto& updateUv = model.geometry.updateUVs[index];
			glm::mat4 m(1.0f);
			switch (weightType) {
				case WeightType::BoneDeform1: {
					m = model.skeleton.transforms[boneIndices[0]];
					break;
				}
				case WeightType::BoneDeform2: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					m = model.skeleton.transforms[i0] * w0 + model.skeleton.transforms[i1] * w1;
					break;
				}
				case WeightType::BoneDeform4: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto i2 = boneIndices[2], i3 = boneIndices[3];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					const auto w2 = boneWeights[2], w3 = boneWeights[3];
					m = model.skeleton.transforms[i0] * w0 + model.skeleton.transforms[i1] * w1
						+ model.skeleton.transforms[i2] * w2 + model.skeleton.transforms[i3] * w3;
					break;
				}
				case WeightType::SphericalDeform: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = 1.0f - w0;
					const auto center = sphericalDeformC;
					const auto cr0 = sphericalDeformR0;
					const auto cr1 = sphericalDeformR1;
					const auto q0 = glm::quat_cast(model.skeleton.nodes[i0]->global);
					const auto q1 = glm::quat_cast(model.skeleton.nodes[i1]->global);
					const auto rotMat = glm::mat3_cast(glm::slerp(q0, q1, w1));
					const auto m0 = model.skeleton.transforms[i0], m1 = model.skeleton.transforms[i1];
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
							glm::mat3x4(glm::transpose(model.skeleton.transforms[boneId]))
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

	void ModelPose::SetupParallelUpdate() const {
		if (!model.geometry.parallelUpdateCount)
			model.geometry.parallelUpdateCount = (std::max)(1u, std::thread::hardware_concurrency());
		model.geometry.parallelUpdateCount = std::min<size_t>(model.geometry.parallelUpdateCount, 16);
		model.geometry.updateRanges.resize(model.geometry.parallelUpdateCount);
		model.geometry.parallelUpdateFutures.resize(model.geometry.parallelUpdateCount - 1);
		const size_t totalVertexCount = model.geometry.positions.size();
		constexpr size_t lowerVertexCount = 1000;
		if (totalVertexCount < model.geometry.updateRanges.size() * lowerVertexCount) {
			const size_t numRanges = (totalVertexCount + lowerVertexCount - 1) / lowerVertexCount;
			for (size_t i = 0; i < model.geometry.updateRanges.size(); i++) {
				auto& [vertexOffset, vertexCount] = model.geometry.updateRanges[i];
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
		const size_t numVertexCount = totalVertexCount / model.geometry.updateRanges.size();
		size_t offset = 0;
		for (size_t i = 0; i < model.geometry.updateRanges.size(); i++) {
			auto& [vertexOffset, vertexCount] = model.geometry.updateRanges[i];
			vertexOffset = offset;
			vertexCount  = numVertexCount + (i == 0 ? totalVertexCount % model.geometry.updateRanges.size() : 0);
			offset += vertexCount;
		}
	}

	void ModelPose::UpdateNodeAnimation(const bool afterPhysicsAnim) const {
		const auto Pred = [&](const std::reference_wrapper<Node>& node) {
			return node.get().isDeformAfterPhysics == afterPhysicsAnim;
		};
		for (auto& nodeRef : model.skeleton.sortedNodes | std::views::filter(Pred))
			nodeRef.get().UpdateLocalTransform();
		for (auto& nodeRef : model.skeleton.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeRef.get();
			if (node.parent.expired())
				node.UpdateGlobalTransform();
		}
		for (auto& nodeRef : model.skeleton.sortedNodes | std::views::filter(Pred)) {
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
		for (const auto& rb : model.physicsData.rigidBodies) {
			rb->ApplyActivation(false);
			rb->ResetTransform();
		}
		model.physicsData.physics->world->stepSimulation(
			1.0f / 60.0f, model.physicsData.physics->maxSubStepCount,
			static_cast<btScalar>(1.0f / model.physicsData.physics->fps));
		for (const auto& rb : model.physicsData.rigidBodies) {
			rb->ReflectGlobalTransform();
			rb->CalcLocalTransform();
		}
		for (const auto& node : model.skeleton.nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
		for (const auto& rb : model.physicsData.rigidBodies)
			rb->Reset(model.physicsData.physics.get());
	}

	void ModelPose::UpdatePhysicsAnimation(const float elapsed) const {
		for (const auto& rb : model.physicsData.rigidBodies)
			rb->ApplyActivation(true);
		model.physicsData.physics->world->stepSimulation(
			elapsed, model.physicsData.physics->maxSubStepCount,
			static_cast<btScalar>(1.0f / model.physicsData.physics->fps));
		for (const auto& rb : model.physicsData.rigidBodies) {
			rb->ReflectGlobalTransform();
			rb->CalcLocalTransform();
		}
		for (const auto& node : model.skeleton.nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
	}

	void ModelPose::Update() const {
		for (size_t i = 0; i < model.skeleton.nodes.size(); i++)
			model.skeleton.transforms[i] = model.skeleton.nodes[i]->global * model.skeleton.nodes[i]->inverseInit;
		if (model.geometry.parallelUpdateCount != model.geometry.updateRanges.size())
			SetupParallelUpdate();
		const size_t futureCount = model.geometry.parallelUpdateFutures.size();
		for (size_t i = 0; i < futureCount; i++) {
			if (model.geometry.updateRanges[i + 1].vertexCount != 0) {
				model.geometry.parallelUpdateFutures[i] = std::async(std::launch::async,
					[this, range = model.geometry.updateRanges[i + 1]] { UpdateSkinning(range); });
			}
		}
		UpdateSkinning(model.geometry.updateRanges[0]);
		for (size_t i = 0; i < futureCount; i++) {
			if (model.geometry.updateRanges[i + 1].vertexCount != 0)
				model.geometry.parallelUpdateFutures[i].wait();
		}
	}
}
