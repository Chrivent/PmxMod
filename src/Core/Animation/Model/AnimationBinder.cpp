#include "Core/Animation/Model/AnimationBinder.h"

#include "Core/Model/Model.h"

namespace Chrivent {
	std::shared_ptr<Node> AnimationBinder::FindNodeByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto it = std::ranges::find_if(model->skeletonData.nodes,
			[&name](const std::shared_ptr<Node>& node) {
				return node && node->name == name;
		});
		return it != model->skeletonData.nodes.end() ? *it : nullptr;
	}

	std::shared_ptr<IkSolver> AnimationBinder::FindIkSolverByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto it = std::ranges::find_if(model->skeletonData.ikSolvers,
			[&name](const std::shared_ptr<IkSolver>& solver) {
				if (!solver)
					return false;
				const auto ikNode = solver->ikNode.lock();
				return ikNode && ikNode->name == name;
		});
		return it != model->skeletonData.ikSolvers.end() ? *it : nullptr;
	}

	std::shared_ptr<Morph> AnimationBinder::FindMorphByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto it = std::ranges::find_if(model->morphData.morphs,
			[&name](const auto& morph) {
				return morph && morph->name == name;
		});
		if (it == model->morphData.morphs.end())
			return nullptr;
		return std::shared_ptr<Morph>(model, it->get());
	}
}
