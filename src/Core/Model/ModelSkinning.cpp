#include "Core/Model/ModelSkinning.h"

#include "Core/Model/Model.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

namespace Chrivent {
	void ModelSkinning::SetupParallelUpdate(Model& model) {
		const std::size_t totalVertexCount = model.geometryData.positions.size();
		if (totalVertexCount == 0) {
			model.geometryData.updateRanges.clear();
			return;
		}
		constexpr std::size_t minimumVertexCount = 1000;
		const std::size_t hardwareWorkerCount = std::max(1u, std::thread::hardware_concurrency());
		const std::size_t workerCount = std::min<std::size_t>(hardwareWorkerCount, 16);
		const std::size_t requiredRangeCount = (totalVertexCount + minimumVertexCount - 1) / minimumVertexCount;
		const std::size_t rangeCount = std::min(workerCount, requiredRangeCount);
		model.geometryData.updateRanges.resize(rangeCount);
		const std::size_t verticesPerRange = totalVertexCount / rangeCount;
		const std::size_t remainder = totalVertexCount % rangeCount;
		std::size_t offset = 0;
		for (std::size_t i = 0; i < rangeCount; i++) {
			auto& [vertexOffset, vertexCount] = model.geometryData.updateRanges[i];
			vertexOffset = offset;
			vertexCount = verticesPerRange + (i < remainder ? 1 : 0);
			offset += vertexCount;
		}
	}

	void ModelSkinning::UpdateRange(Model& model, const std::size_t rangeIndex) {
		const auto& [vertexOffset, vertexCount] = model.geometryData.updateRanges[rangeIndex];
		for (std::size_t index = vertexOffset; index < vertexOffset + vertexCount; index++) {
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
					const auto& nodes = model.skeletonData.GetNodes();
					const auto q0 = glm::quat_cast(nodes[i0]->global);
					const auto q1 = glm::quat_cast(nodes[i1]->global);
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
				const glm::vec3 transformedNormal = glm::mat3(m) * normal;
				const float normalLengthSquared = glm::dot(transformedNormal, transformedNormal);
				updateNormal = std::isfinite(normalLengthSquared) &&
					normalLengthSquared > std::numeric_limits<float>::epsilon()
					? transformedNormal / std::sqrt(normalLengthSquared) : glm::vec3(0);
			}
			updateUv = uv + glm::vec2(morphUv.x, morphUv.y);
		}
	}

	void ModelSkinning::PrepareUpdate(Model& model, const bool preservePreviousPositions) {
		if (preservePreviousPositions &&
			model.geometryData.updatePositions.size() == model.geometryData.positions.size())
			model.geometryData.previousPositions = model.geometryData.updatePositions;
		if (model.geometryData.positions.empty()) {
			model.geometryData.updateRanges.clear();
			return;
		}
		if (!model.geometryData.updateRanges.empty()) {
			const auto& [vertexOffset, vertexCount] = model.geometryData.updateRanges.back();
			if (vertexOffset + vertexCount == model.geometryData.positions.size())
				return;
		}
		SetupParallelUpdate(model);
	}
}
