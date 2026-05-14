#include "ModelSkinning.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

namespace Chrivent::ModelSkinning {
	void UpdateSkinning(const SkinningContext& context, const UpdateRange& range) {
		for (size_t index = range.vertexOffset; index < range.vertexOffset + range.vertexCount; index++) {
			const auto& position = context.positions[index];
			const auto& normal = context.normals[index];
			const auto& uv = context.uvs[index];
			const auto& morphPos = context.morphPositions[index];
			const auto& morphUv = context.morphUVs[index];
			const auto& [weightType, boneIndices, boneWeights,
				sphericalDeformC, sphericalDeformR0,
				sphericalDeformR1] = context.vertexBoneInfos[index];
			auto& updatePos = context.updatePositions[index];
			auto& updateNormal = context.updateNormals[index];
			auto& updateUv = context.updateUVs[index];
			glm::mat4 m(1.0f);
			switch (weightType) {
				case WeightType::BoneDeform1: {
					m = context.transforms[boneIndices[0]];
					break;
				}
				case WeightType::BoneDeform2: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					m = context.transforms[i0] * w0 + context.transforms[i1] * w1;
					break;
				}
				case WeightType::BoneDeform4: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto i2 = boneIndices[2], i3 = boneIndices[3];
					const auto w0 = boneWeights[0], w1 = boneWeights[1];
					const auto w2 = boneWeights[2], w3 = boneWeights[3];
					m = context.transforms[i0] * w0 + context.transforms[i1] * w1
						+ context.transforms[i2] * w2 + context.transforms[i3] * w3;
					break;
				}
				case WeightType::SphericalDeform: {
					const auto i0 = boneIndices[0], i1 = boneIndices[1];
					const auto w0 = boneWeights[0], w1 = 1.0f - w0;
					const auto center = sphericalDeformC;
					const auto cr0 = sphericalDeformR0;
					const auto cr1 = sphericalDeformR1;
					const auto q0 = glm::quat_cast(context.nodes[i0]->global);
					const auto q1 = glm::quat_cast(context.nodes[i1]->global);
					const auto rotMat = glm::mat3_cast(glm::slerp(q0, q1, w1));
					const auto m0 = context.transforms[i0], m1 = context.transforms[i1];
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
							glm::mat3x4(glm::transpose(context.transforms[boneId]))
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
