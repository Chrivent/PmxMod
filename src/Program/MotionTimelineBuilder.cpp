#include "Program/MotionTimelineBuilder.h"

#include "Core/Animation/Model/Animation.h"
#include "Core/Model/Model.h"
#include "Core/Text/TextEncoding.h"
#include "Program/Language.h"
#include "Viewer/Instance/Instance.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>

namespace Chrivent {
	void MotionTimelineBuilder::NormalizeKeys(std::vector<MotionTimelineKey>& keys) {
		std::ranges::sort(keys, {}, &MotionTimelineKey::frame);
		std::vector<MotionTimelineKey> normalized;
		normalized.reserve(keys.size());
		for (auto& key : keys) {
			if (!normalized.empty() && normalized.back().frame == key.frame) {
				if (normalized.back().curves.empty() && !key.curves.empty()) {
					normalized.back().curves = std::move(key.curves);
					normalized.back().values = std::move(key.values);
				}
				continue;
			}
			normalized.emplace_back(std::move(key));
		}
		keys = std::move(normalized);
	}

	std::vector<int> MotionTimelineBuilder::CollectFrames(const std::vector<MotionTimelineRow>& rows) {
		std::vector<int> frames;
		for (const auto& row : rows) {
			for (const auto& key : row.keys)
				frames.emplace_back(key.frame);
		}
		std::ranges::sort(frames);
		const auto uniqueFrames = std::ranges::unique(frames);
		frames.erase(uniqueFrames.begin(), uniqueFrames.end());
		return frames;
	}

	int MotionTimelineBuilder::ToTimelineFrame(const uint32_t frame) {
		constexpr uint32_t maximumFrame = std::numeric_limits<int>::max();
		return frame > maximumFrame ? std::numeric_limits<int>::max() : static_cast<int>(frame);
	}

	void MotionTimelineBuilder::AppendCameraGroup(
		const std::span<const CameraAnimationKey> cameraKeys, std::vector<MotionTimelineGroup>& groups) {
		if (cameraKeys.empty())
			return;
		MotionTimelineRow cameraRow{
			.name = Language::Text("motion.camera"),
			.curveNames = {
				Language::Text("interpolation.x"),
				Language::Text("interpolation.y"),
				Language::Text("interpolation.z"),
				Language::Text("interpolation.rotation"),
				Language::Text("interpolation.distance"),
				Language::Text("interpolation.fov")
			},
			.expandable = true
		};
		cameraRow.keys.reserve(cameraKeys.size());
		for (const auto& [frame, interest, rotate, distance, fov
			, ixBezier, iyBezier, izBezier, rotateBezier, distanceBezier, fovBezier] : cameraKeys) {
			cameraRow.keys.push_back({
				.frame = ToTimelineFrame(frame),
				.curves = {
					ixBezier.GetControlPoints(),
					iyBezier.GetControlPoints(),
					izBezier.GetControlPoints(),
					rotateBezier.GetControlPoints(),
					distanceBezier.GetControlPoints(),
					fovBezier.GetControlPoints()
				},
				.values = {
					interest.x,
					interest.y,
					interest.z,
					glm::degrees(glm::length(rotate)),
					distance,
					glm::degrees(fov)
				}
			});
		}
		MotionTimelineGroup cameraGroup{
			.name = Language::Text("motion.camera"),
			.rows = { std::move(cameraRow) },
			.mode = MotionTimelineMode::Camera,
			.grouped = false
		};
		cameraGroup.keyFrames = CollectFrames(cameraGroup.rows);
		groups.emplace_back(std::move(cameraGroup));
	}

	MotionTimelineData MotionTimelineBuilder::BuildModel(const Instance& instance,
		const std::span<const CameraAnimationKey> cameraKeys, const std::filesystem::path& fallbackModelPath) {
		const Model& model = instance.GetModel();
		const Animation* animation = instance.GetAnimation();
		std::unordered_map<const Node*, std::vector<MotionTimelineKey>> nodeKeys;
		std::unordered_map<const IkSolver*, std::vector<MotionTimelineKey>> ikKeys;
		std::unordered_map<const Morph*, std::vector<MotionTimelineKey>> morphKeys;
		if (animation && animation->IsBoundTo(model)) {
			const auto& nodes = model.skeletonData.GetNodes();
			for (const auto& [targetIndex, keys] : animation->GetNodeTracks()) {
				if (targetIndex >= nodes.size() || !nodes[targetIndex])
					continue;
				const Node* node = nodes[targetIndex].get();
				auto& timelineKeys = nodeKeys[node];
				timelineKeys.reserve(keys.size());
				for (const auto& [frame, translate, rotate
					, txBezier, tyBezier, tzBezier, rotBezier] : keys) {
					glm::quat rotation = glm::normalize(rotate);
					if (rotation.w < 0.0f)
						rotation = -rotation;
					timelineKeys.push_back({
						.frame = ToTimelineFrame(frame),
						.curves = {
							txBezier.GetControlPoints(),
							tyBezier.GetControlPoints(),
							tzBezier.GetControlPoints(),
							rotBezier.GetControlPoints()
						},
						.values = {
							translate.x,
							translate.y,
							translate.z,
							glm::degrees(glm::angle(rotation))
						}
					});
				}
			}
			const auto& ikSolvers = model.skeletonData.GetIkSolvers();
			for (const auto& [targetIndex, keys] : animation->GetIkTracks()) {
				if (targetIndex >= ikSolvers.size() || !ikSolvers[targetIndex])
					continue;
				auto& timelineKeys = ikKeys[ikSolvers[targetIndex].get()];
				timelineKeys.reserve(keys.size());
				for (const auto& [frame, ikEnable] : keys)
					timelineKeys.push_back({ .frame = ToTimelineFrame(frame) });
			}
			const auto& morphs = model.morphData.GetMorphs();
			for (const auto& [targetIndex, keys] : animation->GetMorphTracks()) {
				if (targetIndex >= morphs.size() || !morphs[targetIndex])
					continue;
				auto& timelineKeys = morphKeys[morphs[targetIndex].get()];
				timelineKeys.reserve(keys.size());
				for (const auto& [frame, morphWeight] : keys)
					timelineKeys.push_back({ .frame = ToTimelineFrame(frame) });
			}
		}
		MotionTimelineData timeline;
		timeline.groups.reserve(model.skeletonData.displayFrames.size() + 1);
		AppendCameraGroup(cameraKeys, timeline.groups);
		for (const auto& [name, boneIndices, morphIndices] : model.skeletonData.displayFrames) {
			MotionTimelineGroup group{ .name = TextEncoding::Utf8ToWideOrEmpty(name) };
			group.rows.reserve(boneIndices.size() + morphIndices.size());
			for (const uint32_t boneIndex : boneIndices) {
				if (boneIndex >= model.skeletonData.GetNodes().size())
					continue;
				const auto& node = model.skeletonData.GetNodes()[boneIndex];
				if (!node)
					continue;
				auto keys = nodeKeys[node.get()];
				if (node->ikSolver) {
					const auto& solverKeys = ikKeys[node->ikSolver];
					keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
				}
				NormalizeKeys(keys);
				group.rows.push_back({
					.name = TextEncoding::Utf8ToWideOrEmpty(node->name),
					.curveNames = {
						Language::Text("interpolation.x"),
						Language::Text("interpolation.y"),
						Language::Text("interpolation.z"),
						Language::Text("interpolation.rotation")
					},
					.keys = std::move(keys),
					.expandable = true
				});
			}
			for (const uint32_t morphIndex : morphIndices) {
				if (morphIndex >= model.morphData.GetMorphs().size())
					continue;
				const auto& morph = model.morphData.GetMorphs()[morphIndex];
				if (!morph)
					continue;
				auto keys = morphKeys[morph.get()];
				NormalizeKeys(keys);
				group.rows.push_back({
					.name = TextEncoding::Utf8ToWideOrEmpty(morph->name),
					.keys = std::move(keys)
				});
			}
			if (group.rows.empty())
				continue;
			group.keyFrames = CollectFrames(group.rows);
			timeline.groups.emplace_back(std::move(group));
		}
		if (timeline.groups.size() == (cameraKeys.empty() ? 0 : 1)) {
			MotionTimelineGroup boneGroup{ .name = Language::Text("motion.bones") };
			for (const auto& node : model.skeletonData.GetNodes()) {
				if (!node)
					continue;
				auto keys = nodeKeys[node.get()];
				if (node->ikSolver) {
					const auto& solverKeys = ikKeys[node->ikSolver];
					keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
				}
				NormalizeKeys(keys);
				boneGroup.rows.push_back({
					.name = TextEncoding::Utf8ToWideOrEmpty(node->name),
					.curveNames = {
						Language::Text("interpolation.x"),
						Language::Text("interpolation.y"),
						Language::Text("interpolation.z"),
						Language::Text("interpolation.rotation")
					},
					.keys = std::move(keys),
					.expandable = true
				});
			}
			boneGroup.keyFrames = CollectFrames(boneGroup.rows);
			if (!boneGroup.rows.empty())
				timeline.groups.emplace_back(std::move(boneGroup));
			MotionTimelineGroup morphGroup{ .name = Language::Text("motion.morphs") };
			for (const auto& morph : model.morphData.GetMorphs()) {
				if (!morph)
					continue;
				auto keys = morphKeys[morph.get()];
				NormalizeKeys(keys);
				morphGroup.rows.push_back({
					.name = TextEncoding::Utf8ToWideOrEmpty(morph->name),
					.keys = std::move(keys)
				});
			}
			morphGroup.keyFrames = CollectFrames(morphGroup.rows);
			if (!morphGroup.rows.empty())
				timeline.groups.emplace_back(std::move(morphGroup));
		}
		timeline.name = TextEncoding::Utf8ToWideOrEmpty(model.infoData.modelName);
		if (timeline.name.empty())
			timeline.name = fallbackModelPath.filename().wstring();
		return timeline;
	}

	MotionTimelineData MotionTimelineBuilder::BuildCamera(
		const std::span<const CameraAnimationKey> cameraKeys, const std::filesystem::path& cameraMotionPath) {
		MotionTimelineData timeline;
		AppendCameraGroup(cameraKeys, timeline.groups);
		timeline.name = cameraMotionPath.empty()
			? Language::Text("motion.camera") : cameraMotionPath.filename().wstring();
		return timeline;
	}
}
