#include "ModelUpdater.h"

#include "../Animation/Model/Animation.h"
#include "ModelAnimator.h"
#include "ModelPose.h"
#include "ModelSkinning.h"

#include <chrono>

namespace Chrivent {
	void ModelUpdater::Prepare(
		const Animation* animation,
		const float frame,
		const float physicsElapsed,
		const bool updatePhysics,
		ModelUpdateTiming* timing) const {
		const ModelAnimator animator(model);
		const ModelPose pose(model);
		if (!timing) {
			animator.BeginAnimation();
			if (animation)
				animation->Evaluate(frame);
			animator.UpdateMorphAnimation();
			pose.UpdateNodeAnimation(false);
			if (updatePhysics)
				pose.UpdatePhysicsAnimation(physicsElapsed);
			pose.UpdateNodeAnimation(true);
			pose.UpdateTransforms();
			const ModelSkinning skinning(model);
			skinning.PrepareUpdate();
			return;
		}
		const auto Measure = [](const auto& task) {
			const auto start = std::chrono::steady_clock::now();
			task();
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		};
		timing->initializeMilliseconds = Measure([&] { animator.BeginAnimation(); });
		timing->animationEvaluateMilliseconds = Measure([&] { if (animation) animation->Evaluate(frame); });
		timing->morphMilliseconds = Measure([&] { animator.UpdateMorphAnimation(); });
		timing->beforePhysicsPoseMilliseconds = Measure([&] { pose.UpdateNodeAnimation(false); });
		timing->physicsMilliseconds = Measure([&] { if (updatePhysics) pose.UpdatePhysicsAnimation(physicsElapsed); });
		timing->afterPhysicsPoseMilliseconds = Measure([&] { pose.UpdateNodeAnimation(true); });
		timing->transformMilliseconds = Measure([&] { pose.UpdateTransforms(); });
		const ModelSkinning skinning(model);
		skinning.PrepareUpdate();
	}

	std::size_t ModelUpdater::GetSkinningTaskCount() const {
		const ModelSkinning skinning(model);
		return skinning.GetUpdateRangeCount();
	}

	void ModelUpdater::UpdateSkinning(const std::size_t taskIndex) const {
		const ModelSkinning skinning(model);
		skinning.UpdateRange(taskIndex);
	}
}
