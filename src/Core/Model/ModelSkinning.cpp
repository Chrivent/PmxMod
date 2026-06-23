#include "Core/Model/ModelSkinning.h"

#include <algorithm>
#include <thread>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

namespace Chrivent {
	void ModelSkinning::SetupParallelUpdate() const {
		if (!model.geometryData.parallelUpdateCount)
			model.geometryData.parallelUpdateCount = std::max(1u, std::thread::hardware_concurrency());
		model.geometryData.parallelUpdateCount = std::min<size_t>(model.geometryData.parallelUpdateCount, 16);
		model.geometryData.updateRanges.resize(model.geometryData.parallelUpdateCount);
		const size_t totalVertexCount = model.geometryData.positions.size();
		constexpr size_t lowerVertexCount = 1000;
		if (totalVertexCount < model.geometryData.updateRanges.size() * lowerVertexCount) {
			const size_t numRanges = (totalVertexCount + lowerVertexCount - 1) / lowerVertexCount;
			for (size_t i = 0; i < model.geometryData.updateRanges.size(); i++) {
				auto& [vertexOffset, vertexCount] = model.geometryData.updateRanges[i];
				if (i < numRanges) {
					vertexOffset = i * lowerVertexCount;
					vertexCount  = std::min(lowerVertexCount, totalVertexCount - vertexOffset);
				} else {
					vertexOffset = 0;
					vertexCount = 0;
				}
			}
			return;
		}
		const size_t numVertexCount = totalVertexCount / model.geometryData.updateRanges.size();
		size_t offset = 0;
		for (size_t i = 0; i < model.geometryData.updateRanges.size(); i++) {
			auto& [vertexOffset, vertexCount] = model.geometryData.updateRanges[i];
			vertexOffset = offset;
			vertexCount  = numVertexCount + (i == 0 ? totalVertexCount % model.geometryData.updateRanges.size() : 0);
			offset += vertexCount;
		}
	}

	void ModelSkinning::UpdateRange(const std::size_t rangeIndex) const {
		const auto& [vertexOffset, vertexCount] = model.geometryData.updateRanges[rangeIndex];
		for (size_t index = vertexOffset; index < vertexOffset + vertexCount; index++) {
			const auto& position = model.geometryData.positions[index];
			const auto& normal = model.geometryData.normals[index];
			const auto& uv = model.geometryData.uvs[index];
			const auto& morphPos = model.morphData.morphPositions[index];
			const auto& morphUv = model.morphData.morphUVs[index];
			const auto& [weightType, boneIndices, boneWeights,
				sphericalDeformC, sphericalDeformR0,
				sphericalDeformR1] = model.geometryData.vertexBoneInfos[index];
			auto& updatePos = model.geometryData.updatePositions[index];
			auto& updateNormal = model.geometryData.updateNormals[index];
			auto& updateUv = model.geometryData.updateUVs[index];
			glm::mat4 m(1.0f);
			switch (weightType) {
				case WeightType::BoneDeform1: {
					m = model.skeletonData.transforms[boneIndices[0]];
					break;
				}
				case WeightType::BoneDeform2: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					m = model.skeletonData.transforms[i0] * w0 + model.skeletonData.transforms[i1] * w1;
					break;
				}
				case WeightType::BoneDeform4: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto i2 = boneIndices[2], i3 = boneIndices[3];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					const auto w2 = boneWeights[2], w3 = boneWeights[3];
					m = model.skeletonData.transforms[i0] * w0 + model.skeletonData.transforms[i1] * w1
						+ model.skeletonData.transforms[i2] * w2 + model.skeletonData.transforms[i3] * w3;
					break;
				}
				case WeightType::SphericalDeform: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = 1.0f - w0;
					const auto center = sphericalDeformC;
					const auto cr0 = sphericalDeformR0;
					const auto cr1 = sphericalDeformR1;
					const auto q0 = glm::quat_cast(model.skeletonData.nodes[i0]->global);
					const auto q1 = glm::quat_cast(model.skeletonData.nodes[i1]->global);
					const auto rotMat = glm::mat3_cast(glm::slerp(q0, q1, w1));
					const auto m0 = model.skeletonData.transforms[i0], m1 = model.skeletonData.transforms[i1];
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
						dq[bi] = glm::normalize(glm::dualquat_cast(glm::mat3x4(glm::transpose(model.skeletonData.transforms[boneId]))));
						w[bi] = boneWeights[bi];
					}
					if (glm::dot(dq[0].real, dq[1].real) < 0.0f)
						w[1] *= -1.0f;
					if (glm::dot(dq[0].real, dq[2].real) < 0.0f)
						w[2] *= -1.0f;
					if (glm::dot(dq[0].real, dq[3].real) < 0.0f)
						w[3] *= -1.0f;
					const auto blendDq = glm::normalize(w[0] * dq[0] + w[1] * dq[1] + w[2] * dq[2] + w[3] * dq[3]);
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

	void ModelSkinning::PrepareUpdate() const {
		if (model.geometryData.updateRanges.empty() ||
			model.geometryData.parallelUpdateCount != model.geometryData.updateRanges.size())
			SetupParallelUpdate();
	}
}
