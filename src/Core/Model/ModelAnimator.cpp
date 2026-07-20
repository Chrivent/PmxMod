#include "Core/Model/ModelAnimator.h"

#include "Core/Model/ModelPose.h"
#include "Core/Animation/Model/Animation.h"

namespace Chrivent {
	void ModelAnimator::InitializeAnimation() const {
		ClearBaseAnimation();
		BeginAnimation();
		for (const auto& morph : model.morphData.GetMorphs())
			morph->weight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->enable = true;
		const ModelPose pose(model);
		pose.UpdateNodeAnimation(false);
		pose.UpdateNodeAnimation(true);
		pose.ResetPhysics();
	}

	void ModelAnimator::SaveBaseAnimation() const {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->baseAnimTranslate = node->animTranslate;
			node->baseAnimRotate = node->animRotate;
		}
		for (const auto& morph : model.morphData.GetMorphs())
			morph->saveAnimWeight = morph->weight;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->baseAnimEnable = ikSolver->enable;
	}

	void ModelAnimator::ClearBaseAnimation() const {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->baseAnimTranslate = glm::vec3(0);
			node->baseAnimRotate = glm::quat(1, 0, 0, 0);
		}
		for (const auto& morph : model.morphData.GetMorphs())
			morph->saveAnimWeight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->baseAnimEnable = true;
	}

	void ModelAnimator::BeginAnimation() const {
		for (const auto& node : model.skeletonData.GetNodes())
			node->BeginUpdateTransform();
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		std::ranges::fill(model.morphData.morphPositions, glm::vec3(0));
		std::ranges::fill(model.morphData.morphUVs, glm::vec4(0));
	}

	void ModelAnimator::UpdateMorphAnimation() const {
		model.AccumulateMorphs();
	}

	void ModelAnimator::SyncPhysics(const Animation& anim, const float frame) const {
		if (!model.HasPhysics())
			return;
		SaveBaseAnimation();
		for (int i = 0; i < 30; i++) {
			BeginAnimation();
			anim.Evaluate(frame, (1 + i) / 30.0f);
			UpdateMorphAnimation();
			const ModelPose pose(model);
			pose.UpdateNodeAnimation(false);
			pose.UpdatePhysicsAnimation(1.0f / 30.0f);
			pose.UpdateNodeAnimation(true);
		}
	}
}
