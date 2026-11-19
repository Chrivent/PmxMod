#include "Core/Model/ModelUpdater.h"

#include "Core/Animation/Model/Animation.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelSkinning.h"
#include "Core/Model/Physics/ScenePhysics.h"

#include <algorithm>

namespace Chrivent {
	void ModelUpdater::SaveBaseAnimation(const Model& model) {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->baseAnimTranslate = node->animTranslate;
			node->baseAnimRotate = node->animRotate;
		}
		for (const auto& morph : model.morphData.GetMorphs())
			morph->saveAnimWeight = morph->weight;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->baseAnimEnable = ikSolver->enable;
	}

	void ModelUpdater::ClearBaseAnimation(const Model& model) {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->baseAnimTranslate = glm::vec3(0);
			node->baseAnimRotate = glm::quat(1, 0, 0, 0);
		}
		for (const auto& morph : model.morphData.GetMorphs())
			morph->saveAnimWeight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->baseAnimEnable = true;
	}

	void ModelUpdater::BeginAnimation(Model& model) {
		for (const auto& node : model.skeletonData.GetNodes()) {
			node->BeginUpdateTransform();
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
		}
		std::ranges::fill(model.morphData.morphPositions, glm::vec3(0));
		std::ranges::fill(model.morphData.morphUVs, glm::vec4(0));
	}

	void ModelUpdater::UpdateStageTransforms(const Model& model, const bool afterPhysics) {
		for (const auto& entry : model.skeletonData.sortedNodes) {
			Node& node = entry.get();
			if (node.isDeformAfterPhysics == afterPhysics)
				node.UpdateLocalTransform();
		}
		for (const auto& entry : model.skeletonData.sortedNodes) {
			Node& node = entry.get();
			if (node.isDeformAfterPhysics == afterPhysics && (!node.parent || node.parent->isDeformAfterPhysics != afterPhysics))
				node.UpdateGlobalTransform();
		}
	}

	void ModelUpdater::ApplyStageConstraints(const Model& model, const bool afterPhysics) {
		for (const auto& entry : model.skeletonData.sortedNodes) {
			Node& node = entry.get();
			if (node.isDeformAfterPhysics != afterPhysics)
				continue;
			if (node.appendNode) {
				node.UpdateAppendTransform();
				node.UpdateGlobalTransform();
			}
			if (node.ikSolver) {
				node.ikSolver->Solve();
				node.UpdateGlobalTransform();
			}
		}
	}

	void ModelUpdater::UpdateTransforms(Model& model) {
		const auto& nodes = model.skeletonData.GetNodes();
		for (std::size_t index = 0; index < nodes.size(); index++)
			model.skeletonData.transforms[index] = nodes[index]->global * nodes[index]->inverseInit;
	}

	void ModelUpdater::InitializeAnimation(Model& model) {
		ClearBaseAnimation(model);
		BeginAnimation(model);
		for (const auto& morph : model.morphData.GetMorphs())
			morph->weight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->enable = true;
		UpdateStageTransforms(model, false);
		ApplyStageConstraints(model, false);
		EvaluateAfterPhysics(model, false, nullptr);
	}

	void ModelUpdater::ResetPhysicsAtFrame(const ScenePhysics& scene, const std::span<const ModelAnimationTarget> targets, const float frame) {
		bool hasBodies = false;
		for (const auto& [model, animation] : targets) {
			EvaluateBeforePhysics(model, animation, frame, 1.0f, nullptr);
			EvaluateAfterPhysics(model, false, nullptr);
			if (model.HasPhysics()) {
				SaveBaseAnimation(model);
				hasBodies = true;
			}
		}
		scene.Reset();
		constexpr int warmUpStepCount = 30;
		constexpr float warmUpElapsed = 1.0f / warmUpStepCount;
		for (int index = 0; hasBodies && index < warmUpStepCount; index++) {
			const float weight = static_cast<float>(index + 1) / warmUpStepCount;
			for (const auto& [model, animation] : targets) {
				if (model.HasPhysics())
					EvaluateBeforePhysics(model, animation, frame, weight, nullptr);
			}
			static_cast<void>(scene.Step(warmUpElapsed));
			for (const auto& [model, animation] : targets) {
				if (model.HasPhysics())
					EvaluateAfterPhysics(model, true, nullptr);
			}
		}
		for (const auto& [model, animation] : targets)
			UpdateTransforms(model);
	}

	void ModelUpdater::EvaluateBeforePhysics(Model& model, const Animation* animation, const float frame,
		const float weight, ModelUpdateTiming* timing) {
		RunStage(timing ? &timing->initializeMilliseconds : nullptr,
			[&] { BeginAnimation(model); });
		RunStage(timing ? &timing->animationEvaluateMilliseconds : nullptr,
			[&] { if (animation) animation->Evaluate(frame, weight); });
		RunStage(timing ? &timing->morphMilliseconds : nullptr,
			[&] { model.AccumulateMorphs(); });
		RunStage(timing ? &timing->beforePhysicsPoseMilliseconds : nullptr,
			[&] { UpdateStageTransforms(model, false); ApplyStageConstraints(model, false); });
	}

	void ModelUpdater::EvaluateAfterPhysics(const Model& model, const bool reflectPhysics, ModelUpdateTiming* timing) {
		RunStage(timing ? &timing->afterPhysicsPoseMilliseconds : nullptr,
			[&] { UpdateStageTransforms(model, true); });
		if (reflectPhysics && model.HasPhysics())
			RunStage(timing ? &timing->physicsMilliseconds : nullptr, [&] { model.ReflectPhysics(); });
		RunStage(timing ? &timing->afterPhysicsPoseMilliseconds : nullptr,
			[&] { ApplyStageConstraints(model, true); });
	}

	void ModelUpdater::PrepareBeforePhysics(Model& model, const Animation* animation, const float frame, ModelUpdateTiming* timing) {
		if (timing)
			*timing = {};
		EvaluateBeforePhysics(model, animation, frame, 1.0f, timing);
	}

	void ModelUpdater::PrepareAfterPhysics(Model& model, const bool reflectPhysics, const bool preservePreviousPositions, ModelUpdateTiming* timing) {
		EvaluateAfterPhysics(model, reflectPhysics, timing);
		RunStage(timing ? &timing->transformMilliseconds : nullptr,
			[&] { UpdateTransforms(model); });
		ModelSkinning::PrepareUpdate(model, preservePreviousPositions);
	}

	std::size_t ModelUpdater::CalculateSkinningTaskCount(const Model& model) {
		return model.geometryData.updateRanges.size();
	}

	void ModelUpdater::UpdateSkinning(Model& model, const std::size_t taskIndex) {
		ModelSkinning::UpdateRange(model, taskIndex);
	}
}
