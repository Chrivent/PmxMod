#include "AnimationBinder.h"

#include "../../Model/Model.h"

namespace Chrivent {
	std::shared_ptr<Node> AnimationBinder::FindNodeByName(const std::string& name) const {
		const auto it = std::ranges::find_if(animation.model->nodes,
			[&name](const std::shared_ptr<Node>& node) {
				return node && node->name == name;
		});
		return it != animation.model->nodes.end() ? *it : nullptr;
	}

	std::shared_ptr<IkSolver> AnimationBinder::FindIkSolverByName(const std::string& name) const {
		const auto it = std::ranges::find_if(animation.model->ikSolvers,
			[&name](const std::shared_ptr<IkSolver>& solver) {
				if (!solver)
					return false;
				const auto ikNode = solver->ikNode.lock();
				return ikNode && ikNode->name == name;
		});
		return it != animation.model->ikSolvers.end() ? *it : nullptr;
	}

	std::shared_ptr<Morph> AnimationBinder::FindMorphByName(const std::string& name) const {
		const auto it = std::ranges::find_if(animation.model->morphs,
			[&name](const auto& morph) {
				return morph && morph->name == name;
		});
		if (it == animation.model->morphs.end())
			return nullptr;
		return std::shared_ptr<Morph>(animation.model, it->get());
	}
}
