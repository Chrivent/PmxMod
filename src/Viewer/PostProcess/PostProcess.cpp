#include "Viewer/PostProcess/PostProcess.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace Chrivent {
	size_t PostProcess::ResolveNextHistoryIndex(const size_t currentIndex) {
		return 1 - currentIndex;
	}

	const std::vector<ResourceHistoryState>& PostProcess::ResolveHistoryStates() const {
		return historyFramePending ? pendingResourceHistoryStates : resourceHistoryStates;
	}

	std::vector<ResourceHistoryState>& PostProcess::ResolveHistoryStates() {
		return historyFramePending ? pendingResourceHistoryStates : resourceHistoryStates;
	}

	int PostProcess::ResolveResourceExtent(
		const int fullExtent, const PostProcessResourcePlan& resource, const bool width) {
		if (resource.resolution == EffectPassResolution::Fixed)
			return static_cast<int>(width ? resource.width : resource.height);
		if (resource.resolution == EffectPassResolution::Quarter)
			return std::max(1, (fullExtent + 3) / 4);
		if (resource.resolution == EffectPassResolution::Eighth)
			return std::max(1, (fullExtent + 7) / 8);
		return resource.resolution == EffectPassResolution::Half
			? std::max(1, (fullExtent + 1) / 2) : fullExtent;
	}

	void PostProcess::ResolveOutputExtent(
		const PostProcessPassRoute& route, int& width, int& height) const {
		if (route.outputKind == PostProcessOutputKind::Present
			|| route.outputResourceIndex >= resourcePlans.size())
			return;
		const PostProcessResourcePlan& plan = resourcePlans[route.outputResourceIndex];
		width = ResolveResourceExtent(width, plan, true);
		height = ResolveResourceExtent(height, plan, false);
	}

	size_t PostProcess::ResolveResourceReadIndex(
		const size_t resourceIndex, const size_t transientIndex) const {
		const auto& historyStates = ResolveHistoryStates();
		if (resourceIndex >= resourcePlans.size() || resourceIndex >= historyStates.size()
			|| resourcePlans[resourceIndex].lifetime != EffectResourceLifetime::History)
			return transientIndex;
		return historyStates[resourceIndex].readIndex;
	}

	size_t PostProcess::ResolveResourceWriteIndex(
		const size_t resourceIndex, const size_t transientIndex) const {
		const auto& historyStates = ResolveHistoryStates();
		if (resourceIndex >= resourcePlans.size() || resourceIndex >= historyStates.size()
			|| resourcePlans[resourceIndex].lifetime != EffectResourceLifetime::History)
			return transientIndex;
		return ResolveNextHistoryIndex(historyStates[resourceIndex].readIndex);
	}

	bool PostProcess::NeedsHistoryInitialization(const size_t resourceIndex) const {
		const auto& historyStates = ResolveHistoryStates();
		return resourceIndex < resourcePlans.size() && resourceIndex < historyStates.size()
			&& resourcePlans[resourceIndex].lifetime == EffectResourceLifetime::History
			&& !historyStates[resourceIndex].initialized;
	}

	void PostProcess::MarkHistoryInitialized(const size_t resourceIndex) {
		auto& historyStates = ResolveHistoryStates();
		if (resourceIndex >= resourcePlans.size() || resourceIndex >= historyStates.size()
			|| resourcePlans[resourceIndex].lifetime != EffectResourceLifetime::History)
			return;
		historyStates[resourceIndex].readIndex = 0;
		historyStates[resourceIndex].initialized = true;
	}

	void PostProcess::AdvanceHistory(const PostProcessPassRoute& route) {
		auto& historyStates = ResolveHistoryStates();
		if (route.outputKind != PostProcessOutputKind::Resource
			|| route.outputResourceIndex >= resourcePlans.size()
			|| route.outputResourceIndex >= historyStates.size()
			|| resourcePlans[route.outputResourceIndex].lifetime != EffectResourceLifetime::History)
			return;
		auto& state = historyStates[route.outputResourceIndex];
		state.readIndex = ResolveNextHistoryIndex(state.readIndex);
		state.initialized = true;
	}

	void PostProcess::BeginHistoryFrame() {
		pendingResourceHistoryStates = resourceHistoryStates;
		historyFramePending = true;
	}

	void PostProcess::CommitHistoryFrame() {
		if (!historyFramePending)
			return;
		resourceHistoryStates.swap(pendingResourceHistoryStates);
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
	}

	void PostProcess::DiscardHistoryFrame() {
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
	}

	void PostProcess::SwapExecutionPlan(PostProcess& other) noexcept {
		passDefinitions.swap(other.passDefinitions);
		passRoutes.swap(other.passRoutes);
		resourcePlans.swap(other.resourcePlans);
		resourceHistoryStates.swap(other.resourceHistoryStates);
		pendingResourceHistoryStates.swap(other.pendingResourceHistoryStates);
		std::swap(depthRequired, other.depthRequired);
		std::swap(velocityRequired, other.velocityRequired);
		std::swap(historyFramePending, other.historyFramePending);
	}

	bool PostProcess::BuildExecutionPlan(const std::vector<const EffectRuntimeDefinition*>& effects) {
		std::vector<const EffectRuntimeDefinition*> activeEffects;
		for (const auto* effect : effects) {
			if (effect != nullptr && effect->type == EffectType::PostProcess && !effect->passes.empty())
				activeEffects.push_back(effect);
		}
		std::vector<EffectPassDefinition> passes;
		std::vector<PostProcessPassRoute> routes;
		std::vector<PostProcessResourcePlan> resources;
		PostProcessPassInputRoute effectInput{ .kind = PostProcessInputKind::SceneColor };
		bool requiresDepth = false;
		bool requiresVelocity = false;
		for (size_t effectIndex = 0; effectIndex < activeEffects.size(); effectIndex++) {
			const EffectRuntimeDefinition& effect = *activeEffects[effectIndex];
			PostProcessParameterData parameterData;
			for (const auto& parameter : effect.parameters)
				parameterData.values[parameter.slot] = parameter.defaultValue;
			std::unordered_map<std::string, size_t> resourceIndices;
			for (const auto& [name, lifetime
				, format, resolution
				, width, height] : effect.resources) {
				const size_t index = resources.size();
				resourceIndices.emplace(name, index);
				resources.emplace_back(PostProcessResourcePlan{
					.lifetime = lifetime,
					.format = format,
					.resolution = resolution,
					.width = width,
					.height = height
				});
			}
			const bool lastEffect = effectIndex + 1 == activeEffects.size();
			size_t effectOutputIndex = 0;
			if (!lastEffect) {
				effectOutputIndex = resources.size();
				resources.emplace_back(PostProcessResourcePlan{
					.lifetime = EffectResourceLifetime::Transient,
					.format = EffectTextureFormat::Rgba8Unorm,
					.resolution = EffectPassResolution::Full
				});
			}
			for (size_t passIndex = 0; passIndex < effect.passes.size(); passIndex++) {
				const EffectPassDefinition& pass = effect.passes[passIndex];
				PostProcessPassRoute route;
				route.parameters = parameterData;
				for (const auto& [slot, resource] : pass.inputs) {
					PostProcessPassInputRoute inputRoute{ .slot = slot };
					if (resource == "effect_input")
						inputRoute = effectInput, inputRoute.slot = slot;
					else if (resource == "scene_color")
						inputRoute.kind = PostProcessInputKind::SceneColor;
					else if (resource == "scene_depth") {
						inputRoute.kind = PostProcessInputKind::SceneDepth;
						requiresDepth = true;
					} else if (resource == "scene_velocity") {
						inputRoute.kind = PostProcessInputKind::SceneVelocity;
						requiresVelocity = true;
					} else {
						std::string resourceName = resource;
						if (resourceName.ends_with(".read"))
							resourceName.resize(resourceName.size() - 5);
						const auto findResource = resourceIndices.find(resourceName);
						if (findResource == resourceIndices.end())
							return false;
						inputRoute.kind = PostProcessInputKind::Resource;
						inputRoute.resourceIndex = findResource->second;
					}
					route.inputs.emplace_back(inputRoute);
				}
				if (pass.output == "effect_output") {
					if (passIndex + 1 != effect.passes.size())
						return false;
					route.outputKind = lastEffect ? PostProcessOutputKind::Present : PostProcessOutputKind::Resource;
					route.outputResourceIndex = effectOutputIndex;
				} else {
					std::string resourceName = pass.output;
					if (resourceName.ends_with(".write"))
						resourceName.resize(resourceName.size() - 6);
					const auto resource = resourceIndices.find(resourceName);
					if (resource == resourceIndices.end())
						return false;
					route.outputKind = PostProcessOutputKind::Resource;
					route.outputResourceIndex = resource->second;
				}
				passes.emplace_back(pass);
				routes.emplace_back(std::move(route));
			}
			if (!lastEffect) {
				effectInput.kind = PostProcessInputKind::Resource;
				effectInput.resourceIndex = effectOutputIndex;
			}
		}
		passDefinitions = std::move(passes);
		passRoutes = std::move(routes);
		resourcePlans = std::move(resources);
		resourceHistoryStates.assign(resourcePlans.size(), {});
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
		depthRequired = requiresDepth;
		velocityRequired = requiresVelocity;
		return true;
	}

	bool PostProcess::SetEffects(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return BuildExecutionPlan(effects);
	}

	void PostProcess::ClearEffects() {
		passDefinitions.clear();
		passRoutes.clear();
		resourcePlans.clear();
		resourceHistoryStates.clear();
		pendingResourceHistoryStates.clear();
		depthRequired = false;
		velocityRequired = false;
		historyFramePending = false;
	}

	void PostProcess::ResetHistory() {
		for (auto& state : resourceHistoryStates)
			state = {};
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
	}

	void PostProcess::Clear() {
		ResetResources();
		ClearEffects();
	}
}
