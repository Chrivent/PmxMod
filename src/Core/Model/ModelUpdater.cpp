#include "Core/Model/ModelUpdater.h"

#include "Core/Animation/Model/Animation.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelSkinning.h"

#include <algorithm>
#include <chrono>
#include <ranges>

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

	void ModelUpdater::UpdateNodeAnimation(Model& model, const bool afterPhysicsAnimation) {
		const auto Pred = [&](const std::reference_wrapper<Node>& node) {
			return node.get().isDeformAfterPhysics == afterPhysicsAnimation;
		};
		for (auto& nodeReference : model.skeletonData.sortedNodes | std::views::filter(Pred))
			nodeReference.get().UpdateLocalTransform();
		for (auto& nodeReference : model.skeletonData.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeReference.get();
			if (!node.parent)
				node.UpdateGlobalTransform();
		}
		for (auto& nodeReference : model.skeletonData.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeReference.get();
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
		for (size_t index = 0; index < nodes.size(); index++)
			model.skeletonData.transforms[index] = nodes[index]->global * nodes[index]->inverseInit;
	}

	void ModelUpdater::InitializeAnimation(Model& model) {
		ClearBaseAnimation(model);
		BeginAnimation(model);
		for (const auto& morph : model.morphData.GetMorphs())
			morph->weight = 0;
		for (const auto& ikSolver : model.skeletonData.GetIkSolvers())
			ikSolver->enable = true;
		UpdateNodeAnimation(model, false);
		UpdateNodeAnimation(model, true);
		model.ResetPhysics();
	}

	void ModelUpdater::SyncPhysics(Model& model, const Animation& animation, const float frame) {
		if (!model.HasPhysics())
			return;
		constexpr int warmUpStepCount = 30;
		constexpr float warmUpElapsed = 1.0f / warmUpStepCount;
		SaveBaseAnimation(model);
		for (int index = 0; index < warmUpStepCount; index++) {
			BeginAnimation(model);
			animation.Evaluate(frame, static_cast<float>(1 + index) / warmUpStepCount);
			model.AccumulateMorphs();
			UpdateNodeAnimation(model, false);
			model.UpdatePhysics(warmUpElapsed);
			UpdateNodeAnimation(model, true);
		}
	}

	void ModelUpdater::ResetPhysicsAtFrame(Model& model, const Animation& animation, const float frame) {
		BeginAnimation(model);
		animation.Evaluate(frame);
		model.AccumulateMorphs();
		UpdateNodeAnimation(model, false);
		UpdateNodeAnimation(model, true);
		model.ResetPhysics();
		SyncPhysics(model, animation, frame);
	}

	void ModelUpdater::Prepare(Model& model, const ModelUpdateOptions& options) {
		const auto RunStage = [](double* elapsedMilliseconds, const auto& task) {
			if (!elapsedMilliseconds) {
				task();
				return;
			}
			const auto start = std::chrono::steady_clock::now();
			task();
			*elapsedMilliseconds =
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		};
		ModelUpdateTiming* timing = options.timing;
		RunStage(timing ? &timing->initializeMilliseconds : nullptr,
			[&] { BeginAnimation(model); });
		RunStage(timing ? &timing->animationEvaluateMilliseconds : nullptr,
			[&] { if (options.animation) options.animation->Evaluate(options.frame); });
		RunStage(timing ? &timing->morphMilliseconds : nullptr,
			[&] { model.AccumulateMorphs(); });
		RunStage(timing ? &timing->beforePhysicsPoseMilliseconds : nullptr,
			[&] { UpdateNodeAnimation(model, false); });
		RunStage(timing ? &timing->physicsMilliseconds : nullptr,
			[&] { if (options.updatePhysics) model.UpdatePhysics(options.physicsElapsed); });
		RunStage(timing ? &timing->afterPhysicsPoseMilliseconds : nullptr,
			[&] { UpdateNodeAnimation(model, true); });
		RunStage(timing ? &timing->transformMilliseconds : nullptr,
			[&] { UpdateTransforms(model); });
		ModelSkinning::PrepareUpdate(model, options.preservePreviousPositions);
	}

	std::size_t ModelUpdater::CalculateSkinningTaskCount(const Model& model) {
		return model.geometryData.updateRanges.size();
	}

	void ModelUpdater::UpdateSkinning(Model& model, const std::size_t taskIndex) {
		ModelSkinning::UpdateRange(model, taskIndex);
	}
}
