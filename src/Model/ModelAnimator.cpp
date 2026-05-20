#include "ModelAnimator.h"

#include "ModelMorph.h"
#include "ModelPose.h"
#include "../Animation/Model/Animation.h"

namespace Chrivent {
	void ModelAnimator::InitializeAnimation() const {
		ClearBaseAnimation();
		for (const auto& node : model.skeletonData.nodes) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		BeginAnimation();
		for (const auto& morph : model.morphData.morphs)
			morph->weight = 0;
		for (const auto& ikSolver : model.skeletonData.ikSolvers)
			ikSolver->info.enable = true;
		const ModelPose pose(model);
		pose.UpdateNodeAnimation(false);
		pose.UpdateNodeAnimation(true);
		pose.ResetPhysics();
	}

	void ModelAnimator::SaveBaseAnimation() const {
		for (const auto& node : model.skeletonData.nodes) {
			node->baseAnimTranslate = node->animTranslate;
			node->baseAnimRotate = node->animRotate;
		}
		for (const auto& morph : model.morphData.morphs)
			morph->saveAnimWeight = morph->weight;
		for (const auto& ikSolver : model.skeletonData.ikSolvers)
			ikSolver->info.baseAnimEnable = ikSolver->info.enable;
	}

	void ModelAnimator::ClearBaseAnimation() const {
		for (const auto& node : model.skeletonData.nodes) {
			node->baseAnimTranslate = glm::vec3(0);
			node->baseAnimRotate = glm::quat(1, 0, 0, 0);
		}
		for (const auto& morph : model.morphData.morphs)
			morph->saveAnimWeight = 0;
		for (const auto& ikSolver : model.skeletonData.ikSolvers)
			ikSolver->info.baseAnimEnable = true;
	}

	void ModelAnimator::BeginAnimation() const {
		for (const auto& node : model.skeletonData.nodes)
			node->BeginUpdateTransform();
		for (const auto& node : model.skeletonData.nodes) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		std::ranges::fill(model.morphData.morphPositions, glm::vec3(0));
		std::ranges::fill(model.morphData.morphUVs, glm::vec4(0));
	}

	void ModelAnimator::UpdateMorphAnimation() const {
		const ModelMorph morph(model);
		morph.Update();
	}

	void ModelAnimator::UpdateAllAnimation(const Animation* anim, const float frame, const float physicsElapsed, const bool updatePhysics) const {
		if (anim)
			anim->Evaluate(frame);
		UpdateMorphAnimation();
		const ModelPose pose(model);
		pose.UpdateNodeAnimation(false);
		if (updatePhysics)
			pose.UpdatePhysicsAnimation(physicsElapsed);
		pose.UpdateNodeAnimation(true);
	}

	void ModelAnimator::SyncPhysics(const Animation& anim, const float frame) const {
		SaveBaseAnimation();
		for (int i = 0; i < 30; i++) {
			BeginAnimation();
			anim.Evaluate(frame, static_cast<float>(1 + i) / 30.0f);
			UpdateMorphAnimation();
			const ModelPose pose(model);
			pose.UpdateNodeAnimation(false);
			pose.UpdatePhysicsAnimation(1.0f / 30.0f);
			pose.UpdateNodeAnimation(true);
		}
	}
}
