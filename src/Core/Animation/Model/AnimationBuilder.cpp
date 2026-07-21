#include "Core/Animation/Model/AnimationBuilder.h"

#include "Core/Animation/AnimationKeySequence.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelCoordinateConverter.h"
#include "Core/Text/TextEncoding.h"

#include <ranges>

namespace Chrivent {
	Node* AnimationBuilder::FindNodeByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto& nodes = model->skeletonData.GetNodes();
		const auto it = std::ranges::find_if(nodes, [&name](const std::shared_ptr<Node>& node) {
			return node && node->name == name;
		});
		return it != nodes.end() ? it->get() : nullptr;
	}

	IkSolver* AnimationBuilder::FindIkSolverByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto& ikSolvers = model->skeletonData.GetIkSolvers();
		const auto it = std::ranges::find_if(ikSolvers, [&name](const std::shared_ptr<IkSolver>& solver) {
			if (!solver)
				return false;
			const auto ikNode = solver->ikNode.lock();
			return ikNode && ikNode->name == name;
		});
		return it != ikSolvers.end() ? it->get() : nullptr;
	}

	Morph* AnimationBuilder::FindMorphByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto& morphs = model->morphData.GetMorphs();
		const auto it = std::ranges::find_if(morphs, [&name](const auto& morph) {
			return morph && morph->name == name;
		});
		return it != morphs.end() ? it->get() : nullptr;
	}

	template <typename Track, typename KeyMap, typename Resolver>
	std::vector<Track> AnimationBuilder::TakeTracks(KeyMap& keysByName, const Resolver& resolveTarget) {
		std::vector<Track> tracks;
		tracks.reserve(keysByName.size());
		for (auto& [name, keys] : keysByName) {
			auto* target = resolveTarget(name);
			if (!target)
				continue;
			AnimationKeySequence::SortAndKeepLastKeyPerFrame(keys);
			tracks.emplace_back(Track{target, std::move(keys)});
		}
		keysByName.clear();
		return tracks;
	}

	NodeAnimationKey AnimationBuilder::CreateNodeAnimationKey(const VmdParser::VmdMotion& motion) {
		NodeAnimationKey key{};
		key.frame = motion.frame;
		key.translate = motion.translate * glm::vec3(1, 1, -1);
		const glm::quat q = glm::normalize(motion.quaternion);
		const auto rot = ModelCoordinateConverter::ConvertZAxis(glm::mat3_cast(q));
		key.rotate = glm::normalize(glm::quat_cast(rot));
		key.txBezier.Assign(motion.interpolation[0], motion.interpolation[8], motion.interpolation[4], motion.interpolation[12]);
		key.tyBezier.Assign(motion.interpolation[1], motion.interpolation[9], motion.interpolation[5], motion.interpolation[13]);
		key.tzBezier.Assign(motion.interpolation[2], motion.interpolation[10], motion.interpolation[6], motion.interpolation[14]);
		key.rotBezier.Assign(motion.interpolation[3], motion.interpolation[11], motion.interpolation[7], motion.interpolation[15]);
		return key;
	}

	void AnimationBuilder::AddNodeAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.motions.empty())
			return;
		for (const auto& motion : vmdData.motions) {
			auto nodeName = TextEncoding::ShiftJisToUtf8(motion.boneName, sizeof(motion.boneName));
			nodeKeysByName[nodeName].emplace_back(CreateNodeAnimationKey(motion));
		}
	}

	void AnimationBuilder::AddIkAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.iks.empty())
			return;
		for (const auto& ik : vmdData.iks) {
			for (const auto& [name, enable] : ik.ikStates) {
				auto ikName = TextEncoding::ShiftJisToUtf8(name, sizeof(name));
				auto& keys = ikKeysByName[ikName];
				auto& [frame, ikEnable] = keys.emplace_back();
				frame = ik.frame;
				ikEnable = enable != 0;
			}
		}
	}

	void AnimationBuilder::AddMorphAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.morphs.empty())
			return;
		for (const auto& [blendShapeName, frame, weight] : vmdData.morphs) {
			auto morphName = TextEncoding::ShiftJisToUtf8(blendShapeName, sizeof(blendShapeName));
			auto& keys = morphKeysByName[morphName];
			auto& [keyFrame, morphWeight] = keys.emplace_back();
			keyFrame = frame;
			morphWeight = weight;
		}
	}

	void AnimationBuilder::Build(const VmdParser::VmdData& vmdData) {
		AddNodeAnimations(vmdData);
		AddIkAnimations(vmdData);
		AddMorphAnimations(vmdData);
	}

	std::unique_ptr<Animation> AnimationBuilder::TakeAnimation() {
		const auto ResolveNode = [this](const std::string& name) { return FindNodeByName(name); };
		const auto ResolveIkSolver = [this](const std::string& name) { return FindIkSolverByName(name); };
		const auto ResolveMorph = [this](const std::string& name) { return FindMorphByName(name); };
		auto nodeTracks = TakeTracks<NodeAnimationTrack>(nodeKeysByName, ResolveNode);
		auto ikTracks = TakeTracks<IkAnimationTrack>(ikKeysByName, ResolveIkSolver);
		auto morphTracks = TakeTracks<MorphAnimationTrack>(morphKeysByName, ResolveMorph);
		return std::make_unique<Animation>(
			model, std::move(nodeTracks), std::move(ikTracks), std::move(morphTracks));
	}
}
