#include "ModelUpdater.h"

#include "ModelAnimator.h"
#include "ModelPose.h"
#include "ModelSkinning.h"

namespace Chrivent {
	void ModelUpdater::Prepare(const Animation* animation, const float frame, const float physicsElapsed, const bool updatePhysics) const {
		const ModelAnimator animator(model);
		animator.BeginAnimation();
		animator.UpdateAllAnimation(animation, frame, physicsElapsed, updatePhysics);
		const ModelPose pose(model);
		pose.UpdateTransforms();
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
