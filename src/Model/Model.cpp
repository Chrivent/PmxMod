#include "Model.h"

#include "ModelLoader.h"
#include "ModelMorph.h"
#include "ModelPose.h"
#include "../Animation/Animation.h"

namespace Chrivent {
	Model::~Model() {
		Destroy();
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
		const ModelMorph morph(*this);
		morph.Update();
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
