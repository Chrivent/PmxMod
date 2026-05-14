#include "ModelSkinning.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

namespace Chrivent::ModelSkinning {
	void UpdateSkinning(const SkinningContext& context, const UpdateRange& range) {
		const auto* position = context.positions.data() + range.vertexOffset;
		const auto* normal = context.normals.data() + range.vertexOffset;
		const auto* uv = context.uvs.data() + range.vertexOffset;
		const auto* morphPos = context.morphPositions.data() + range.vertexOffset;
		const auto* morphUv = context.morphUVs.data() + range.vertexOffset;
		const auto* vtxInfo = context.vertexBoneInfos.data() + range.vertexOffset;
		auto* updatePos = context.updatePositions.data() + range.vertexOffset;
		auto* updateNormal = context.updateNormals.data() + range.vertexOffset;
		auto* updateUv = context.updateUVs.data() + range.vertexOffset;
		for (size_t i = 0; i < range.vertexCount; i++, vtxInfo++, position++, normal++, uv++,
			morphPos++, morphUv++, updatePos++, updateNormal++, updateUv++) {
			glm::mat4 m;
			switch (vtxInfo->weightType) {
				case WeightType::BoneDeform1: {
					m = context.transforms[vtxInfo->boneIndices[0]];
					break;
				}
				case WeightType::BoneDeform2: {
					const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
					const auto w0 = vtxInfo->boneWeights[0], w1 = vtxInfo->boneWeights[1];
					m = context.transforms[i0] * w0 + context.transforms[i1] * w1;
					break;
				}
				case WeightType::BoneDeform4: {
					const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
					const auto i2 = vtxInfo->boneIndices[2], i3 = vtxInfo->boneIndices[3];
					const auto w0 = vtxInfo->boneWeights[0], w1 = vtxInfo->boneWeights[1];
					const auto w2 = vtxInfo->boneWeights[2], w3 = vtxInfo->boneWeights[3];
					m = context.transforms[i0] * w0 + context.transforms[i1] * w1 + context.transforms[i2] * w2 + context.transforms[i3] * w3;
					break;
				}
				case WeightType::SphericalDeform: {
					const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
					const auto w0 = vtxInfo->boneWeights[0], w1 = 1.0f - w0;
					const auto center = vtxInfo->sphericalDeformC;
					const auto cr0 = vtxInfo->sphericalDeformR0;
					const auto cr1 = vtxInfo->sphericalDeformR1;
					const auto q0 = glm::quat_cast(context.nodes[i0]->global);
					const auto q1 = glm::quat_cast(context.nodes[i1]->global);
					const auto rotMat = glm::mat3_cast(glm::slerp(q0, q1, w1));
					const auto m0 = context.transforms[i0], m1 = context.transforms[i1];
					const auto pos = *position + *morphPos;
					*updatePos = rotMat * (pos - center)
						+ glm::vec3(m0 * glm::vec4(cr0, 1)) * w0
						+ glm::vec3(m1 * glm::vec4(cr1, 1)) * w1;
					*updateNormal = rotMat * *normal;
					break;
				}
				case WeightType::QuaternionDeform: {
					glm::dualquat dq[4]{};
					float w[4] = {};
					for (int bi = 0; bi < 4; bi++) {
						auto boneId = vtxInfo->boneIndices[bi];
						if (boneId != -1) {
							dq[bi] = glm::normalize(glm::dualquat_cast(glm::mat3x4(glm::transpose(context.transforms[boneId]))));
							w[bi] = vtxInfo->boneWeights[bi];
						}
					}
					if (glm::dot(dq[0].real, dq[1].real) < 0)
						w[1] *= -1.0f;
					if (glm::dot(dq[0].real, dq[2].real) < 0)
						w[2] *= -1.0f;
					if (glm::dot(dq[0].real, dq[3].real) < 0)
						w[3] *= -1.0f;
					auto blendDq = glm::normalize(w[0] * dq[0] + w[1] * dq[1] + w[2] * dq[2] + w[3] * dq[3]);
					m = glm::transpose(glm::mat3x4_cast(blendDq));
					break;
				}
			}
			if (WeightType::SphericalDeform != vtxInfo->weightType) {
				*updatePos = glm::vec3(m * glm::vec4(*position + *morphPos, 1));
				*updateNormal = glm::normalize(glm::mat3(m) * *normal);
			}
			*updateUv = *uv + glm::vec2(morphUv->x, morphUv->y);
		}
	}
}
