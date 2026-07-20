#include "Core/Model/ModelPose.h"
#include <ranges>

namespace Chrivent {
	void ModelPose::UpdateNodeAnimation(const bool afterPhysicsAnim) const {
		const auto Pred = [&](const std::reference_wrapper<Node>& node) {
			return node.get().isDeformAfterPhysics == afterPhysicsAnim;
		};
		for (auto& nodeRef : model.skeletonData.sortedNodes | std::views::filter(Pred))
			nodeRef.get().UpdateLocalTransform();
		for (auto& nodeRef : model.skeletonData.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeRef.get();
			if (node.parent.expired())
				node.UpdateGlobalTransform();
		}
		for (auto& nodeRef : model.skeletonData.sortedNodes | std::views::filter(Pred)) {
			auto& node = nodeRef.get();
			if (!node.appendNode.expired()) {
				node.UpdateAppendTransform();
				node.UpdateGlobalTransform();
			}
			if (const auto ikSolver = node.ikSolver.lock()) {
				ikSolver->Solve();
				node.UpdateGlobalTransform();
			}
		}
	}

	void ModelPose::ResetPhysics() const {
		model.ResetPhysics();
	}

	void ModelPose::UpdatePhysicsAnimation(const float elapsed) const {
		model.UpdatePhysics(elapsed);
	}

	void ModelPose::UpdateTransforms() const {
		for (size_t i = 0; i < model.skeletonData.nodes.size(); i++)
			model.skeletonData.transforms[i] = model.skeletonData.nodes[i]->global * model.skeletonData.nodes[i]->inverseInit;
	}
}
