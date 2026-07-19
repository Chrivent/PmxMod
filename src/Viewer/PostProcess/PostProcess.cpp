#include "Viewer/PostProcess/PostProcess.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Chrivent {
	std::string PostProcessPlanError::Format() const {
		std::string formatted = "효과 " + std::to_string(effectIndex);
		if (passIndex != noPassIndex)
			formatted += "의 패스 " + std::to_string(passIndex);
		formatted += ": ";
		formatted += message;
		return formatted;
	}

	size_t PostProcess::ResolveNextHistoryIndex(const size_t currentIndex) {
		return 1 - currentIndex;
	}

	const std::vector<PostProcess::ResourceHistoryState>& PostProcess::ResolveHistoryStates() const {
		return historyFramePending ? pendingResourceHistoryStates : resourceHistoryStates;
	}

	std::vector<PostProcess::ResourceHistoryState>& PostProcess::ResolveHistoryStates() {
		return historyFramePending ? pendingResourceHistoryStates : resourceHistoryStates;
	}

	PostProcessPlanError PostProcess::CreatePlanError(const PostProcessPlanErrorCode code,
		const size_t effectIndex, std::string message, const size_t passIndex) {
		return {
			.code = code,
			.effectIndex = effectIndex,
			.passIndex = passIndex,
			.message = std::move(message)
		};
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
		effectParameters.swap(other.effectParameters);
		resourcePlans.swap(other.resourcePlans);
		resourceHistoryStates.swap(other.resourceHistoryStates);
		pendingResourceHistoryStates.swap(other.pendingResourceHistoryStates);
		std::swap(depthRequired, other.depthRequired);
		std::swap(velocityRequired, other.velocityRequired);
		std::swap(historyFramePending, other.historyFramePending);
	}

	std::expected<PostProcess::ExecutionPlan, PostProcessPlanError> PostProcess::BuildExecutionPlan(
		const std::vector<const EffectRuntimeDefinition*>& effects) {
		std::vector<ShaderProgramDefinition> plannedPrograms;
		std::vector<PostProcessPassRoute> plannedRoutes;
		std::vector<PostProcessParameterData> plannedParameters;
		std::vector<PostProcessResourcePlan> plannedResources;
		PostProcessPassInputRoute effectInput{ .kind = PostProcessInputKind::SceneColor };
		bool requiresDepth = false;
		bool requiresVelocity = false;
		for (size_t effectIndex = 0; effectIndex < effects.size(); effectIndex++) {
			const EffectRuntimeDefinition* effect = effects[effectIndex];
			if (effect == nullptr || effect->passes.empty()) {
				return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidEffect,
					effectIndex, "효과의 런타임 정의 또는 패스가 비어 있습니다"));
			}
			const auto& [parameters, resources, passes] = *effect;
			PostProcessParameterData parameterData;
			bool usedParameterSlots[PostProcessInputLayout::maxParameterCount]{};
			for (const auto& [slot, value] : parameters) {
				if (slot >= PostProcessInputLayout::maxParameterCount
					|| usedParameterSlots[slot] || !std::isfinite(value)) {
					return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidParameter,
						effectIndex, "파라미터 슬롯 또는 값이 올바르지 않습니다"));
				}
				usedParameterSlots[slot] = true;
				parameterData.values[slot] = value;
			}
			plannedParameters.emplace_back(parameterData);
			const size_t resourceBaseIndex = plannedResources.size();
			for (const auto& [lifetime, format, resolution, width, height] : resources) {
				if (lifetime != EffectResourceLifetime::Transient
					&& lifetime != EffectResourceLifetime::History) {
					return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidResource,
						effectIndex, "리소스 lifetime이 올바르지 않습니다"));
				}
				if (format != EffectTextureFormat::Rgba8Unorm
					&& format != EffectTextureFormat::Rgba16Float
					&& format != EffectTextureFormat::Rgba32Float) {
					return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidResource,
						effectIndex, "리소스 format이 올바르지 않습니다"));
				}
				if (resolution != EffectPassResolution::Full
					&& resolution != EffectPassResolution::Half
					&& resolution != EffectPassResolution::Quarter
					&& resolution != EffectPassResolution::Eighth
					&& resolution != EffectPassResolution::Fixed) {
					return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidResource,
						effectIndex, "리소스 resolution이 올바르지 않습니다"));
				}
				if (resolution == EffectPassResolution::Fixed
					&& (width == 0 || height == 0
						|| width > static_cast<uint32_t>(std::numeric_limits<int>::max())
						|| height > static_cast<uint32_t>(std::numeric_limits<int>::max()))) {
					return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidResource,
						effectIndex, "고정 리소스 크기가 렌더러 범위를 벗어났습니다"));
				}
				plannedResources.emplace_back(PostProcessResourcePlan{
					.lifetime = lifetime,
					.format = format,
					.resolution = resolution,
					.width = width,
					.height = height
				});
			}
			std::vector<uint8_t> initializedTransientResources(resources.size(), 0);
			const bool lastEffect = effectIndex + 1 == effects.size();
			size_t effectOutputIndex = 0;
			if (!lastEffect) {
				effectOutputIndex = plannedResources.size();
				plannedResources.emplace_back(PostProcessResourcePlan{
					.lifetime = EffectResourceLifetime::Transient,
					.format = EffectTextureFormat::Rgba8Unorm,
					.resolution = EffectPassResolution::Full
				});
			}
			for (size_t passIndex = 0; passIndex < passes.size(); passIndex++) {
				const auto& [program, inputs, output] = passes[passIndex];
				if (program.shaderPath.empty() || program.vertexEntry.empty() || program.pixelEntry.empty()) {
					return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidProgram,
						effectIndex, "셰이더 경로 또는 진입점이 없습니다", passIndex));
				}
				PostProcessPassRoute route;
				route.effectIndex = effectIndex;
				bool usedSlots[PostProcessInputLayout::maxTextureCount]{};
				for (const auto& [slot, kind, resourceIndex] : inputs) {
					if (slot >= PostProcessInputLayout::maxTextureCount) {
						return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidInput,
							effectIndex, "허용 범위를 벗어난 texture 슬롯이 있습니다", passIndex));
					}
					if (usedSlots[slot]) {
						return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidInput,
							effectIndex, "중복된 texture 슬롯이 있습니다", passIndex));
					}
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
						if (kind != EffectPassInputKind::Resource
							|| resourceIndex >= resources.size()) {
							return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidInput,
								effectIndex, "존재하지 않는 입력 리소스를 참조합니다", passIndex));
						}
						if (resources[resourceIndex].lifetime == EffectResourceLifetime::Transient
							&& initializedTransientResources[resourceIndex] == 0) {
							return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidInput,
								effectIndex, "초기화되지 않은 transient 리소스를 읽습니다", passIndex));
						}
						inputRoute.kind = PostProcessInputKind::Resource;
						inputRoute.resourceIndex = resourceBaseIndex + resourceIndex;
					}
					route.inputs.emplace_back(inputRoute);
				}
				if (output.kind == EffectPassOutputKind::EffectOutput) {
					if (passIndex + 1 != passes.size()) {
						return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidOutput,
							effectIndex, "최종 출력은 마지막 패스에서만 사용할 수 있습니다", passIndex));
					}
					route.outputKind = lastEffect ? PostProcessOutputKind::Present : PostProcessOutputKind::Resource;
					route.outputResourceIndex = effectOutputIndex;
				} else {
					if (output.kind != EffectPassOutputKind::Resource
						|| output.resourceIndex >= resources.size()) {
						return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidOutput,
							effectIndex, "존재하지 않는 출력 리소스를 참조합니다", passIndex));
					}
					const EffectResourceDefinition& outputResource = resources[output.resourceIndex];
					if (outputResource.lifetime == EffectResourceLifetime::Transient) {
						for (const auto& input : inputs) {
							if (input.kind == EffectPassInputKind::Resource
								&& input.resourceIndex == output.resourceIndex) {
								return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidOutput,
									effectIndex, "같은 transient 리소스를 동시에 읽고 씁니다", passIndex));
							}
						}
						initializedTransientResources[output.resourceIndex] = 1;
					}
					route.outputKind = PostProcessOutputKind::Resource;
					route.outputResourceIndex = resourceBaseIndex + output.resourceIndex;
				}
				plannedPrograms.emplace_back(program);
				plannedRoutes.emplace_back(std::move(route));
			}
			if (passes.back().output.kind != EffectPassOutputKind::EffectOutput) {
				return std::unexpected(CreatePlanError(PostProcessPlanErrorCode::InvalidOutput,
					effectIndex, "마지막 패스가 effect output에 쓰지 않습니다", passes.size() - 1));
			}
			if (!lastEffect) {
				effectInput.kind = PostProcessInputKind::Resource;
				effectInput.resourceIndex = effectOutputIndex;
			}
		}
		return ExecutionPlan{
			.shaderPrograms = std::move(plannedPrograms),
			.passRoutes = std::move(plannedRoutes),
			.effectParameters = std::move(plannedParameters),
			.resourcePlans = std::move(plannedResources),
			.depthRequired = requiresDepth,
			.velocityRequired = requiresVelocity
		};
	}

	std::expected<void, PostProcessPlanError> PostProcess::SetEffects(
		const std::vector<const EffectRuntimeDefinition*>& effects) {
		auto planResult = BuildExecutionPlan(effects);
		if (!planResult)
			return std::unexpected(planResult.error());
		auto [plannedShaderPrograms, plannedPassRoutes
			, plannedEffectParameters, plannedResourcePlans
			, plannedDepthRequired, plannedVelocityRequired] = std::move(*planResult);
		shaderPrograms = std::move(plannedShaderPrograms);
		passRoutes = std::move(plannedPassRoutes);
		effectParameters = std::move(plannedEffectParameters);
		resourcePlans = std::move(plannedResourcePlans);
		resourceHistoryStates.assign(resourcePlans.size(), {});
		pendingResourceHistoryStates.clear();
		historyFramePending = false;
		depthRequired = plannedDepthRequired;
		velocityRequired = plannedVelocityRequired;
		return {};
	}

	std::expected<void, PostProcessPlanError> PostProcess::ValidateEffects(
		const std::vector<const EffectRuntimeDefinition*>& effects) {
		const auto planResult = BuildExecutionPlan(effects);
		if (!planResult)
			return std::unexpected(planResult.error());
		return {};
	}

	bool PostProcess::ValidateParameterUpdates(const std::span<const EffectParameterUpdate> updates) const {
		for (const auto& [effectIndex, slot, value] : updates) {
			if (effectIndex >= effectParameters.size() || slot >= PostProcessInputLayout::maxParameterCount
				|| !std::isfinite(value))
				return false;
		}
		return true;
	}

	bool PostProcess::UpdateParameters(const std::span<const EffectParameterUpdate> updates) {
		if (!ValidateParameterUpdates(updates))
			return false;
		for (const auto& [effectIndex, slot, value] : updates) {
			effectParameters[effectIndex].values[slot] = value;
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
