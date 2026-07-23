#include "Core/Animation/Model/AnimationBuilder.h"

#include "Core/Model/Model.h"
#include "Core/Model/ModelCoordinateConverter.h"
#include "Core/Text/TextEncoding.h"

#include <unordered_map>

namespace Chrivent {
	const std::string& AnimationBuilder::ResolveName(const char* encodedName, const std::size_t size) {
		std::size_t length = 0;
		while (length < size && encodedName[length] != '\0')
			length++;
		std::string cacheKey(encodedName, length);
		auto [iterator, inserted] = decodedNames.try_emplace(std::move(cacheKey));
		if (inserted)
			iterator->second = TextEncoding::ShiftJisToUtf8(iterator->first.data(), iterator->first.size());
		return iterator->second;
	}

	template <typename Track, typename KeyMap, typename Resolver>
	std::vector<Track> AnimationBuilder::TakeTracks(KeyMap& keysByName, const Resolver& resolveTarget) {
		std::vector<Track> tracks;
		tracks.reserve(keysByName.size());
		for (auto& [name, keys] : keysByName) {
			auto* target = resolveTarget(name);
			if (!target)
				continue;
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
			const auto& nodeName = ResolveName(motion.boneName, sizeof(motion.boneName));
			nodeKeysByName[nodeName].emplace_back(CreateNodeAnimationKey(motion));
		}
	}

	void AnimationBuilder::AddIkAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.iks.empty())
			return;
		for (const auto& ik : vmdData.iks) {
			for (const auto& [name, enable] : ik.ikStates) {
				const auto& ikName = ResolveName(name, sizeof(name));
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
			const auto& morphName = ResolveName(blendShapeName, sizeof(blendShapeName));
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
		std::unordered_map<std::string, Node*> nodesByName;
		std::unordered_map<std::string, IkSolver*> ikSolversByName;
		std::unordered_map<std::string, Morph*> morphsByName;
		if (model) {
			const auto& nodes = model->skeletonData.GetNodes();
			nodesByName.reserve(nodes.size());
			for (const auto& node : nodes) {
				if (node)
					nodesByName.try_emplace(node->name, node.get());
			}
			const auto& ikSolvers = model->skeletonData.GetIkSolvers();
			ikSolversByName.reserve(ikSolvers.size());
			for (const auto& ikSolver : ikSolvers) {
				if (ikSolver) {
					if (const auto ikNode = ikSolver->ikNode)
						ikSolversByName.try_emplace(ikNode->name, ikSolver.get());
				}
			}
			const auto& morphs = model->morphData.GetMorphs();
			morphsByName.reserve(morphs.size());
			for (const auto& morph : morphs) {
				if (morph)
					morphsByName.try_emplace(morph->name, morph.get());
			}
		}
		const auto ResolveNode = [&nodesByName](const std::string& name) {
			const auto iterator = nodesByName.find(name);
			return iterator != nodesByName.end() ? iterator->second : nullptr;
		};
		const auto ResolveIkSolver = [&ikSolversByName](const std::string& name) {
			const auto iterator = ikSolversByName.find(name);
			return iterator != ikSolversByName.end() ? iterator->second : nullptr;
		};
		const auto ResolveMorph = [&morphsByName](const std::string& name) {
			const auto iterator = morphsByName.find(name);
			return iterator != morphsByName.end() ? iterator->second : nullptr;
		};
		auto nodeTracks = TakeTracks<NodeAnimationTrack>(nodeKeysByName, ResolveNode);
		auto ikTracks = TakeTracks<IkAnimationTrack>(ikKeysByName, ResolveIkSolver);
		auto morphTracks = TakeTracks<MorphAnimationTrack>(morphKeysByName, ResolveMorph);
		return std::make_unique<Animation>(
			model, std::move(nodeTracks), std::move(ikTracks), std::move(morphTracks));
	}
}
