#include "Core/Model/ModelPose.h"
#include "Core/Model/Physics/Physics.h"

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
		if (!model.physicsData->physics || model.physicsData->rigidBodies.empty())
			return;
		for (const auto& rb : model.physicsData->rigidBodies) {
			rb->ApplyActivation(false);
			rb->ResetTransform();
		}
		model.physicsData->physics->Step(1.0f / 60.0f);
		for (const auto& rb : model.physicsData->rigidBodies) {
			rb->ReflectGlobalTransform();
			rb->CalcLocalTransform();
		}
		for (const auto& node : model.skeletonData.nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
		for (const auto& rb : model.physicsData->rigidBodies) {
			model.physicsData->physics->CleanCollisionPairs(*rb->GetRigidBody());
			rb->Reset();
		}
	}

	void ModelPose::UpdatePhysicsAnimation(const float elapsed) const {
		if (!model.physicsData->physics || model.physicsData->rigidBodies.empty())
			return;
		for (const auto& rb : model.physicsData->rigidBodies)
			rb->ApplyActivation(true);
		model.physicsData->physics->Step(elapsed);
		for (const auto& rb : model.physicsData->rigidBodies) {
			rb->ReflectGlobalTransform();
			rb->CalcLocalTransform();
		}
		for (const auto& node : model.skeletonData.nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
	}

	void ModelPose::UpdateTransforms() const {
		for (size_t i = 0; i < model.skeletonData.nodes.size(); i++)
			model.skeletonData.transforms[i] = model.skeletonData.nodes[i]->global * model.skeletonData.nodes[i]->inverseInit;
	}
}
