#include "AnimationBinder.h"

#include "../../Model/Model.h"

namespace Chrivent {
	std::shared_ptr<Node> AnimationBinder::FindNodeByName(const std::string& name) const {
		if (!info.model)
			return nullptr;
		const auto it = std::ranges::find_if(info.model->skeletonData.nodes,
			[&name](const std::shared_ptr<Node>& node) {
				return node && node->GetInfo().name == name;
		});
		return it != info.model->skeletonData.nodes.end() ? *it : nullptr;
	}

	std::shared_ptr<IkSolver> AnimationBinder::FindIkSolverByName(const std::string& name) const {
		if (!info.model)
			return nullptr;
		const auto it = std::ranges::find_if(info.model->skeletonData.ikSolvers,
			[&name](const std::shared_ptr<IkSolver>& solver) {
				if (!solver)
					return false;
				const auto ikNode = solver->GetInfo().ikNode.lock();
				return ikNode && ikNode->GetInfo().name == name;
		});
		return it != info.model->skeletonData.ikSolvers.end() ? *it : nullptr;
	}

	std::shared_ptr<Morph> AnimationBinder::FindMorphByName(const std::string& name) const {
		if (!info.model)
			return nullptr;
		const auto it = std::ranges::find_if(info.model->morphData.morphs,
			[&name](const auto& morph) {
				return morph && morph->name == name;
		});
		if (it == info.model->morphData.morphs.end())
			return nullptr;
		return std::shared_ptr<Morph>(info.model, it->get());
	}
}
