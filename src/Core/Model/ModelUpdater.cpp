#include "Core/Model/ModelUpdater.h"

#include "Core/Animation/Model/Animation.h"
#include "Core/Model/ModelAnimator.h"
#include "Core/Model/ModelPose.h"
#include "Core/Model/ModelSkinning.h"

#include <chrono>

namespace Chrivent {
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
			[&] { ModelAnimator::BeginAnimation(model); });
		RunStage(timing ? &timing->animationEvaluateMilliseconds : nullptr,
			[&] { if (options.animation) options.animation->Evaluate(options.frame); });
		RunStage(timing ? &timing->morphMilliseconds : nullptr,
			[&] { ModelAnimator::UpdateMorphAnimation(model); });
		RunStage(timing ? &timing->beforePhysicsPoseMilliseconds : nullptr,
			[&] { ModelPose::UpdateNodeAnimation(model, false); });
		RunStage(timing ? &timing->physicsMilliseconds : nullptr,
			[&] { if (options.updatePhysics) ModelPose::UpdatePhysicsAnimation(model, options.physicsElapsed); });
		RunStage(timing ? &timing->afterPhysicsPoseMilliseconds : nullptr,
			[&] { ModelPose::UpdateNodeAnimation(model, true); });
		RunStage(timing ? &timing->transformMilliseconds : nullptr,
			[&] { ModelPose::UpdateTransforms(model); });
		ModelSkinning::PrepareUpdate(model, options.preservePreviousPositions);
	}

	std::size_t ModelUpdater::CalculateSkinningTaskCount(const Model& model) {
		return ModelSkinning::GetUpdateRangeCount(model);
	}

	void ModelUpdater::UpdateSkinning(Model& model, const std::size_t taskIndex) {
		ModelSkinning::UpdateRange(model, taskIndex);
	}
}
