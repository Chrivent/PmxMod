#include "Core/Animation/Model/AnimationBinder.h"

#include "Core/Model/Model.h"

namespace Chrivent {
	Node* AnimationBinder::FindNodeByName(const std::string& name) const {
		if (!targetModel)
			return nullptr;
		const auto it = std::ranges::find_if(targetModel->skeletonData.nodes,
			[&name](const std::shared_ptr<Node>& node) {
				return node && node->name == name;
			});
		return it != targetModel->skeletonData.nodes.end() ? it->get() : nullptr;
	}

	IkSolver* AnimationBinder::FindIkSolverByName(const std::string& name) const {
		if (!targetModel)
			return nullptr;
		const auto it = std::ranges::find_if(targetModel->skeletonData.ikSolvers,
			[&name](const std::shared_ptr<IkSolver>& solver) {
				if (!solver)
					return false;
				const auto ikNode = solver->ikNode.lock();
				return ikNode && ikNode->name == name;
			});
		return it != targetModel->skeletonData.ikSolvers.end() ? it->get() : nullptr;
	}

	Morph* AnimationBinder::FindMorphByName(const std::string& name) const {
		if (!targetModel)
			return nullptr;
		const auto it = std::ranges::find_if(targetModel->morphData.morphs,
			[&name](const auto& morph) {
				return morph && morph->name == name;
			});
		if (it == targetModel->morphData.morphs.end())
			return nullptr;
		return it->get();
	}
}
