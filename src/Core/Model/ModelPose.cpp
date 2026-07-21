#include "Core/Model/ModelPose.h"
#include "Core/Model/Model.h"

#include <ranges>

namespace Chrivent {
	void ModelPose::UpdateNodeAnimation(Model& model, const bool afterPhysicsAnim) {
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

	void ModelPose::UpdateTransforms(Model& model) {
		const auto& nodes = model.skeletonData.GetNodes();
		for (size_t i = 0; i < nodes.size(); i++)
			model.skeletonData.transforms[i] = nodes[i]->global * nodes[i]->inverseInit;
	}
}
