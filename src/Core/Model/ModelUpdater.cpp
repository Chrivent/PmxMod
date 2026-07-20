#include "Core/Model/ModelUpdater.h"

#include "Core/Animation/Model/Animation.h"
#include "Core/Model/ModelAnimator.h"
#include "Core/Model/ModelPose.h"
#include "Core/Model/ModelSkinning.h"

#include <chrono>

namespace Chrivent {
	void ModelUpdater::Prepare(const Animation* animation, const float frame, const float physicsElapsed,
		const bool preservePreviousPositions, const bool updatePhysics, ModelUpdateTiming* timing) const {
		const ModelAnimator animator(model);
		const ModelPose pose(model);
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
		RunStage(timing ? &timing->initializeMilliseconds : nullptr, [&] { animator.BeginAnimation(); });
		RunStage(timing ? &timing->animationEvaluateMilliseconds : nullptr,
			[&] { if (animation) animation->Evaluate(frame); });
		RunStage(timing ? &timing->morphMilliseconds : nullptr, [&] { animator.UpdateMorphAnimation(); });
		RunStage(timing ? &timing->beforePhysicsPoseMilliseconds : nullptr,
			[&] { pose.UpdateNodeAnimation(false); });
		RunStage(timing ? &timing->physicsMilliseconds : nullptr,
			[&] { if (updatePhysics) pose.UpdatePhysicsAnimation(physicsElapsed); });
		RunStage(timing ? &timing->afterPhysicsPoseMilliseconds : nullptr,
			[&] { pose.UpdateNodeAnimation(true); });
		RunStage(timing ? &timing->transformMilliseconds : nullptr, [&] { pose.UpdateTransforms(); });
		const ModelSkinning skinning(model);
		skinning.PrepareUpdate(preservePreviousPositions);
	}

	std::size_t ModelUpdater::CalculateSkinningTaskCount() const {
		const ModelSkinning skinning(model);
		return skinning.GetUpdateRangeCount();
	}

	void ModelUpdater::UpdateSkinning(const std::size_t taskIndex) const {
		const ModelSkinning skinning(model);
		skinning.UpdateRange(taskIndex);
	}
}
