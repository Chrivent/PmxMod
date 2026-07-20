#include "Core/Animation/Model/AnimationBinder.h"

#include "Core/Model/Model.h"

namespace Chrivent {
	Node* AnimationBinder::FindNodeByName(const std::string& name) const {
		if (!targetModel)
			return nullptr;
		const auto& nodes = targetModel->skeletonData.GetNodes();
		const auto it = std::ranges::find_if(nodes,
			[&name](const std::shared_ptr<Node>& node) {
				return node && node->name == name;
			});
		return it != nodes.end() ? it->get() : nullptr;
	}

	IkSolver* AnimationBinder::FindIkSolverByName(const std::string& name) const {
		if (!targetModel)
			return nullptr;
		const auto& ikSolvers = targetModel->skeletonData.GetIkSolvers();
		const auto it = std::ranges::find_if(ikSolvers,
			[&name](const std::shared_ptr<IkSolver>& solver) {
				if (!solver)
					return false;
				const auto ikNode = solver->ikNode.lock();
				return ikNode && ikNode->name == name;
			});
		return it != ikSolvers.end() ? it->get() : nullptr;
	}

	Morph* AnimationBinder::FindMorphByName(const std::string& name) const {
		if (!targetModel)
			return nullptr;
		const auto& morphs = targetModel->morphData.GetMorphs();
		const auto it = std::ranges::find_if(morphs,
			[&name](const auto& morph) {
				return morph && morph->name == name;
			});
		if (it == morphs.end())
			return nullptr;
		return it->get();
	}
}
