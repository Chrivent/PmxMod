#include "Core/Model/ModelAnimator.h"

#include "Core/Model/ModelPose.h"
#include "Core/Animation/Model/Animation.h"

namespace Chrivent {
	void ModelAnimator::InitializeAnimation(Model& model) {
		ClearBaseAnimation(model);
		BeginAnimation(model);
		for (const auto& morph : model.morphData.GetMorphs())
			morph->weight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->enable = true;
		ModelPose::UpdateNodeAnimation(model, false);
		ModelPose::UpdateNodeAnimation(model, true);
		model.ResetPhysics();
	}

	void ModelAnimator::SaveBaseAnimation(const Model& model) {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->baseAnimTranslate = node->animTranslate;
			node->baseAnimRotate = node->animRotate;
		}
		for (const auto& morph : model.morphData.GetMorphs())
			morph->saveAnimWeight = morph->weight;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->baseAnimEnable = ikSolver->enable;
	}

	void ModelAnimator::ClearBaseAnimation(const Model& model) {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->baseAnimTranslate = glm::vec3(0);
			node->baseAnimRotate = glm::quat(1, 0, 0, 0);
		}
		for (const auto& morph : model.morphData.GetMorphs())
			morph->saveAnimWeight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->baseAnimEnable = true;
	}

	void ModelAnimator::BeginAnimation(Model& model) {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->BeginUpdateTransform();
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		std::ranges::fill(model.morphData.morphPositions, glm::vec3(0));
		std::ranges::fill(model.morphData.morphUVs, glm::vec4(0));
	}

	void ModelAnimator::UpdateMorphAnimation(const Model& model) {
		model.AccumulateMorphs();
	}

	void ModelAnimator::SyncPhysics(Model& model, const Animation& animation, const float frame) {
		if (!model.HasPhysics())
			return;
		constexpr int warmUpStepCount = 30;
		constexpr float warmUpElapsed = 1.0f / warmUpStepCount;
		SaveBaseAnimation(model);
		for (int i = 0; i < warmUpStepCount; i++) {
			BeginAnimation(model);
			animation.Evaluate(frame, static_cast<float>(1 + i) / warmUpStepCount);
			UpdateMorphAnimation(model);
			ModelPose::UpdateNodeAnimation(model, false);
			model.UpdatePhysics(warmUpElapsed);
			ModelPose::UpdateNodeAnimation(model, true);
		}
	}
}
