#include "Model.h"

#include "ModelLoader.h"
#include "ModelPose.h"
#include "../Animation/Animation.h"

#include <ranges>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Chrivent {
	Model::~Model() {
		Destroy();
	}
	
	void Model::AccumulateMaterialMul(MaterialMorph& out, const MaterialMorph& val, const float weight) {
		out.diffuse = glm::mix(out.diffuse, out.diffuse * val.diffuse, weight);
		out.specular = glm::mix(out.specular, out.specular * val.specular, weight);
		out.specularPower = glm::mix(out.specularPower, out.specularPower * val.specularPower, weight);
		out.ambient = glm::mix(out.ambient, out.ambient * val.ambient, weight);
		out.edgeColor = glm::mix(out.edgeColor, out.edgeColor * val.edgeColor, weight);
		out.edgeSize = glm::mix(out.edgeSize, out.edgeSize * val.edgeSize, weight);
		out.textureFactor = glm::mix(out.textureFactor, out.textureFactor * val.textureFactor, weight);
		out.sphereTextureFactor = glm::mix(out.sphereTextureFactor, out.sphereTextureFactor * val.sphereTextureFactor, weight);
		out.cartoonTextureFactor = glm::mix(out.cartoonTextureFactor, out.cartoonTextureFactor * val.cartoonTextureFactor, weight);
	}

	void Model::AccumulateMaterialAdd(MaterialMorph& out, const MaterialMorph& val, const float weight) {
		out.diffuse += val.diffuse * weight;
		out.specular += val.specular * weight;
		out.specularPower += val.specularPower * weight;
		out.ambient += val.ambient * weight;
		out.edgeColor += val.edgeColor * weight;
		out.edgeSize += val.edgeSize * weight;
		out.textureFactor += val.textureFactor * weight;
		out.sphereTextureFactor += val.sphereTextureFactor * weight;
		out.cartoonTextureFactor += val.cartoonTextureFactor * weight;
	}

	void Model::EvalMorph(const Morph* morph, const float morphWeight) {
		if (std::abs(morphWeight) <= std::numeric_limits<float>::epsilon())
			return;
		switch (morph->morphType) {
			case MorphType::Position:
				MorphPosition(positionMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Uv:
				MorphUv(uvMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Material:
				MorphMaterial(materialMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Bone:
				MorphBone(boneMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Group: {
				for (const auto& [morphIndex, weight] : groupMorphs[morph->dataIndex]) {
					if (morphIndex == -1)
						continue;
					EvalMorph(morphs[morphIndex].get(), weight * morphWeight);
				}
				break;
			}
			case MorphType::AddUv1:
				break;
			case MorphType::AddUv2:
				break;
			case MorphType::AddUv3:
				break;
			case MorphType::AddUv4:
				break;
			case MorphType::Flip:
				break;
			case MorphType::Impulse:
				break;
		}
	}

	void Model::MorphPosition(const std::vector<PositionMorph>& morphData, const float weight) {
		for (const auto& [vertexIndex, position] : morphData)
			morphPositions[vertexIndex] += position * weight;
	}

	void Model::MorphUv(const std::vector<UvMorph>& morphData, const float weight) {
		for (const auto& [vertexIndex, uv] : morphData)
			morphUVs[vertexIndex] += uv * weight;
	}

	void Model::BeginMorphMaterial() {
		constexpr MaterialMorph initMul{
			0, OpType::Mul, glm::vec4(1), glm::vec3(1), 1.0f, glm::vec3(1),
			glm::vec4(1), 1.0f, glm::vec4(1), glm::vec4(1), glm::vec4(1)
		};
		constexpr MaterialMorph initAdd{
			0, OpType::Add, glm::vec4(0), glm::vec3(0), 0.0f, glm::vec3(0),
			glm::vec4(0), 0.0f, glm::vec4(0), glm::vec4(0), glm::vec4(0)
		};
		for (size_t i = 0; i < materials.size(); i++) {
			auto& mul = mulMaterialFactors[i];
			mul = initMul;
			mul.diffuse       = initMaterials[i].diffuse;
			mul.specular      = initMaterials[i].specular;
			mul.specularPower = initMaterials[i].specularPower;
			mul.ambient       = initMaterials[i].ambient;
			addMaterialFactors[i] = initAdd;
		}
	}

	void Model::EndMorphMaterial() {
		for (size_t i = 0; i < materials.size(); i++) {
			auto& mat = materials[i];
			const auto& mul = mulMaterialFactors[i];
			const auto& add = addMaterialFactors[i];
			auto matFactor = mul;
			AccumulateMaterialAdd(matFactor, add, 1.0f);
			mat.diffuse        = matFactor.diffuse;
			mat.specular       = matFactor.specular;
			mat.specularPower  = matFactor.specularPower;
			mat.ambient        = matFactor.ambient;
			mat.textureMulFactor   = mul.textureFactor;
			mat.textureAddFactor   = add.textureFactor;
			mat.sphereTextureMulFactor = mul.sphereTextureFactor;
			mat.sphereTextureAddFactor = add.sphereTextureFactor;
			mat.cartoonTextureMulFactor = mul.cartoonTextureFactor;
			mat.cartoonTextureAddFactor = add.cartoonTextureFactor;
		}
	}

	void Model::MorphMaterial(const std::vector<MaterialMorph>& morphData, const float weight) {
		for (const auto& matMorph : morphData) {
			auto Apply = [&](const size_t mi) {
				switch (matMorph.opType) {
					case OpType::Mul: AccumulateMaterialMul(mulMaterialFactors[mi], matMorph, weight); break;
					case OpType::Add: AccumulateMaterialAdd(addMaterialFactors[mi], matMorph, weight); break;
				}
			};
			if (matMorph.materialIndex != -1)
				Apply(static_cast<size_t>(matMorph.materialIndex));
			else {
				for (size_t i = 0; i < materials.size(); i++)
					Apply(i);
			}
		}
	}

	void Model::MorphBone(const std::vector<BoneMorph>& morphData, const float weight) const {
		for (const auto& [boneIndex, position, quaternion] : morphData) {
			auto* node = nodes[boneIndex].get();
			node->translate += position * weight;
			const glm::quat q = glm::slerp(glm::quat(1,0,0,0), quaternion, weight);
			node->rotate = glm::normalize(q * node->rotate);
		}
	}

	void Model::InitializeAnimation() {
		ClearBaseAnimation();
		for (const auto& node : nodes) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		BeginAnimation();
		for (const auto& morph : morphs)
			morph->weight = 0;
		for (const auto& ikSolver : ikSolvers)
			ikSolver->enable = true;
		const ModelPose pose(*this);
		pose.UpdateNodeAnimation(false);
		pose.UpdateNodeAnimation(true);
		pose.ResetPhysics();
	}

	void Model::SaveBaseAnimation() const {
		for (const auto& node : nodes) {
			node->baseAnimTranslate = node->animTranslate;
			node->baseAnimRotate = node->animRotate;
		}
		for (const auto& morph : morphs)
			morph->saveAnimWeight = morph->weight;
		for (const auto& ikSolver : ikSolvers)
			ikSolver->baseAnimEnable = ikSolver->enable;
	}

	void Model::ClearBaseAnimation() const {
		for (const auto& node : nodes) {
			node->baseAnimTranslate = glm::vec3(0);
			node->baseAnimRotate = glm::quat(1, 0, 0, 0);
		}
		for (const auto& morph : morphs)
			morph->saveAnimWeight = 0;
		for (const auto& ikSolver : ikSolvers)
			ikSolver->baseAnimEnable = true;
	}

	void Model::BeginAnimation() {
		for (const auto& node : nodes)
			node->BeginUpdateTransform();
		for (const auto& node : nodes) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		std::ranges::fill(morphPositions, glm::vec3(0));
		std::ranges::fill(morphUVs, glm::vec4(0));
	}

	void Model::UpdateMorphAnimation() {
		BeginMorphMaterial();
		for (const auto& morph : morphs)
			EvalMorph(morph.get(), morph->weight);
		EndMorphMaterial();
	}

	void Model::UpdateAllAnimation(const Animation* anim, const float frame, const float physicsElapsed) {
		if (anim)
			anim->Evaluate(frame);
		UpdateMorphAnimation();
		const ModelPose pose(*this);
		pose.UpdateNodeAnimation(false);
		pose.UpdatePhysicsAnimation(physicsElapsed);
		pose.UpdateNodeAnimation(true);
	}

	bool Model::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) {
		const ModelLoader loader(*this);
		return loader.Load(filepath, dataDir);
	}

	void Model::Destroy() {
		materials.clear();
		subMeshes.clear();
		positions.clear();
		normals.clear();
		uvs.clear();
		vertexBoneInfos.clear();
		indices.clear();
		sortedNodes.clear();
		nodes.clear();
		updateRanges.clear();
		for (const auto& joint : joints)
			physics->world->removeConstraint(joint->constraint.get());
		joints.clear();
		for (const auto& rb : rigidBodies)
			physics->world->removeRigidBody(rb->rigidBody.get());
		rigidBodies.clear();
		physics.reset();
	}
}
