#include "Viewer/PostProcess/PostProcess.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	size_t PostProcess::ResolveNextHistoryIndex(const size_t currentIndex) {
		return 1 - currentIndex;
	}

	const std::vector<PostProcess::ResourceHistoryState>& PostProcess::ResolveHistoryStates() const {
		return historyFramePending ? pendingResourceHistoryStates : resourceHistoryStates;
	}

	std::vector<PostProcess::ResourceHistoryState>& PostProcess::ResolveHistoryStates() {
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
		shaderPrograms.swap(other.shaderPrograms);
		passRoutes.swap(other.passRoutes);
		resourcePlans.swap(other.resourcePlans);
		resourceHistoryStates.swap(other.resourceHistoryStates);
		pendingResourceHistoryStates.swap(other.pendingResourceHistoryStates);
		std::swap(depthRequired, other.depthRequired);
		std::swap(velocityRequired, other.velocityRequired);
		std::swap(historyFramePending, other.historyFramePending);
		std::swap(effectCount, other.effectCount);
	}

	bool PostProcess::BuildExecutionPlan(const std::vector<const EffectRuntimeDefinition*>& effects) {
		std::vector<const EffectRuntimeDefinition*> activeEffects;
		for (const auto* effect : effects) {
			if (effect != nullptr && !effect->passes.empty())
				activeEffects.push_back(effect);
		}
		std::vector<ShaderProgramDefinition> programs;
		std::vector<PostProcessPassRoute> routes;
		std::vector<PostProcessResourcePlan> resources;
		PostProcessPassInputRoute effectInput{ .kind = PostProcessInputKind::SceneColor };
		bool requiresDepth = false;
		bool requiresVelocity = false;
		for (size_t effectIndex = 0; effectIndex < activeEffects.size(); effectIndex++) {
			const EffectRuntimeDefinition& effect = *activeEffects[effectIndex];
			PostProcessParameterData parameterData;
			for (const auto& parameter : effect.parameters) {
				if (parameter.slot >= PostProcessInputLayout::maxParameterCount)
					return false;
				parameterData.values[parameter.slot] = parameter.value;
			}
			const size_t resourceBaseIndex = resources.size();
			for (const auto& [lifetime, format, resolution, width, height] : effect.resources) {
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
				const auto& [program, inputs, output] = effect.passes[passIndex];
				PostProcessPassRoute route;
				route.parameters = parameterData;
				route.effectIndex = effectIndex;
				bool usedSlots[PostProcessInputLayout::maxTextureCount]{};
				for (const auto& [slot, kind, resourceIndex] : inputs) {
					if (slot >= PostProcessInputLayout::maxTextureCount || usedSlots[slot])
						return false;
					usedSlots[slot] = true;
					PostProcessPassInputRoute inputRoute{ .slot = slot };
					if (kind == EffectPassInputKind::EffectInput) {
						inputRoute = effectInput;
						inputRoute.slot = slot;
					}
					else if (kind == EffectPassInputKind::SceneColor)
						inputRoute.kind = PostProcessInputKind::SceneColor;
					else if (kind == EffectPassInputKind::SceneDepth) {
						inputRoute.kind = PostProcessInputKind::SceneDepth;
						requiresDepth = true;
					} else if (kind == EffectPassInputKind::SceneVelocity) {
						inputRoute.kind = PostProcessInputKind::SceneVelocity;
						requiresVelocity = true;
					} else {
						if (kind != EffectPassInputKind::Resource || resourceIndex >= effect.resources.size())
							return false;
						inputRoute.kind = PostProcessInputKind::Resource;
						inputRoute.resourceIndex = resourceBaseIndex + resourceIndex;
					}
					route.inputs.emplace_back(inputRoute);
				}
				if (output.kind == EffectPassOutputKind::EffectOutput) {
					if (passIndex + 1 != effect.passes.size())
						return false;
					route.outputKind = lastEffect ? PostProcessOutputKind::Present : PostProcessOutputKind::Resource;
					route.outputResourceIndex = effectOutputIndex;
				} else {
					if (output.kind != EffectPassOutputKind::Resource
						|| output.resourceIndex >= effect.resources.size())
						return false;
					route.outputKind = PostProcessOutputKind::Resource;
					route.outputResourceIndex = resourceBaseIndex + output.resourceIndex;
				}
				programs.emplace_back(program);
				routes.emplace_back(std::move(route));
			}
			if (!lastEffect) {
				effectInput.kind = PostProcessInputKind::Resource;
				effectInput.resourceIndex = effectOutputIndex;
			}
		}
		shaderPrograms = std::move(programs);
		passRoutes = std::move(routes);
		resourcePlans = std::move(resources);
		resourceHistoryStates.assign(resourcePlans.size(), {});
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
		depthRequired = requiresDepth;
		velocityRequired = requiresVelocity;
		effectCount = activeEffects.size();
		return true;
	}

	bool PostProcess::SetEffects(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return BuildExecutionPlan(effects);
	}

	bool PostProcess::UpdateParameters(const std::span<const EffectParameterUpdate> updates) {
		for (const auto& [effectIndex, slot, value] : updates) {
			if (effectIndex >= effectCount || slot >= PostProcessInputLayout::maxParameterCount
				|| !std::isfinite(value))
				return false;
		}
		for (const auto& [effectIndex, slot, value] : updates) {
			for (auto& route : passRoutes) {
				if (route.effectIndex == effectIndex)
					route.parameters.values[slot] = value;
			}
		}
		return true;
	}

	void PostProcess::ResetHistory() {
		for (auto& state : resourceHistoryStates)
			state = {};
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
	}
}
