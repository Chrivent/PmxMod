#include "Program/ShaderEffectController.h"

#include "Core/Text/TextEncoding.h"
#include "Program/Language.h"
#include "Program/Manager/PanelManager.h"
#include "Viewer/Viewer/Viewer.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

namespace Chrivent {
	void ShaderEffectController::BuildEffectEntries() {
		effectEntries.clear();
		for (size_t packageIndex = 0; packageIndex < packages.size(); packageIndex++) {
			for (size_t effectIndex = 0; effectIndex < packages[packageIndex].effects.size(); effectIndex++)
				effectEntries.push_back({ .packageIndex = packageIndex, .effectIndex = effectIndex });
		}
	}

	ShaderEffectController::EffectReference ShaderEffectController::ResolveEffect(const size_t effectIndex) const {
		if (effectIndex >= effectEntries.size())
			return {};
		const auto& [packageIndex, packageEffectIndex] = effectEntries[effectIndex];
		if (packageIndex >= packages.size() || packageEffectIndex >= packages[packageIndex].effects.size())
			return {};
		return { .package = &packages[packageIndex], .effect = &packages[packageIndex].effects[packageEffectIndex] };
	}

	void ShaderEffectController::RefreshPanel(PanelManager& panelManager, Viewer& viewer) const {
		std::vector<std::wstring> effectNames;
		effectNames.reserve(effectEntries.size());
		for (size_t effectIndex = 0; effectIndex < effectEntries.size(); effectIndex++) {
			const auto [package, effect] = ResolveEffect(effectIndex);
			if (package && effect) {
				effectNames.emplace_back(TextEncoding::Utf8ToWideOrEmpty(package->name) + L" / "
					+ TextEncoding::Utf8ToWideOrEmpty(effect->name));
			}
		}
		panelManager.ApplyShaderNames(effectNames, selectedEffectIndex, effectEnabled);
		const SceneRenderState& scene = viewer.GetSceneRenderState();
		panelManager.ApplyBuiltInShaderStates(scene.modelEnabled, scene.edgeEnabled, scene.groundShadowEnabled);
	}

	void ShaderEffectController::ShowEffectInformation(PanelManager& panelManager, const size_t effectIndex) const {
		const auto [package, effect] = ResolveEffect(effectIndex);
		if (!package || !effect) {
			panelManager.ClearInformation();
			return;
		}
		std::vector<InformationField> fields{
			{ .labelKey = "information.effect_name", .value = TextEncoding::Utf8ToWideOrEmpty(effect->name) },
			{ .labelKey = "information.effect_id", .value = TextEncoding::Utf8ToWideOrEmpty(effect->id) },
			{ .labelKey = "information.package_name", .value = TextEncoding::Utf8ToWideOrEmpty(package->name) },
			{ .labelKey = "information.package_id", .value = TextEncoding::Utf8ToWideOrEmpty(package->id) },
			{ .labelKey = "information.parameters", .value = std::to_wstring(effect->parameters.size()) },
			{ .labelKey = "information.passes", .value = std::to_wstring(effect->runtime.passes.size()) }
		};
		if (!package->version.empty())
			fields.push_back({ .labelKey = "information.version", .value = TextEncoding::Utf8ToWideOrEmpty(package->version) });
		if (!package->author.empty())
			fields.push_back({ .labelKey = "information.author", .value = TextEncoding::Utf8ToWideOrEmpty(package->author) });
		panelManager.ApplyInformation(std::move(fields));
	}

	void ShaderEffectController::ShowEffectTimeline(PanelManager& panelManager, const size_t effectIndex) const {
		const auto [package, effect] = ResolveEffect(effectIndex);
		if (!package || !effect) {
			panelManager.ApplyMotionTimeline(Language::Text("panel.camera"), {});
			return;
		}
		const std::wstring name = TextEncoding::Utf8ToWideOrEmpty(package->name) + L" / "
			+ TextEncoding::Utf8ToWideOrEmpty(effect->name);
		panelManager.ApplyMotionTimeline(name, {});
	}

	GraphicsError::Result<void> ShaderEffectController::Reload(const std::filesystem::path& packagesDirectory,
		Viewer& viewer, PanelManager& panelManager) {
		std::vector<std::pair<std::string, std::string>> enabledEffectIds;
		enabledEffectIds.reserve(effectEntries.size());
		for (size_t effectIndex = 0; effectIndex < effectEntries.size() && effectIndex < effectEnabled.size(); effectIndex++) {
			if (!effectEnabled[effectIndex])
				continue;
			const auto [package, effect] = ResolveEffect(effectIndex);
			if (package && effect)
				enabledEffectIds.emplace_back(package->id, effect->id);
		}
		std::pair<std::string, std::string> selectedEffectId;
		if (const auto [package, effect] = ResolveEffect(selectedEffectIndex); package && effect)
			selectedEffectId = { package->id, effect->id };

		auto [loadedPackages, errors] = ShaderPackageLoader::Discover(packagesDirectory);
		packages = std::move(loadedPackages);
		for (const auto& error : errors)
			std::cerr << "셰이더 패키지를 불러오지 못했습니다: " << error.Format() << '\n';
		BuildEffectEntries();
		selectedEffectIndex = 0;
		effectEnabled.assign(effectEntries.size(), false);
		for (size_t effectIndex = 0; effectIndex < effectEntries.size(); effectIndex++) {
			const auto [package, effect] = ResolveEffect(effectIndex);
			if (!package || !effect)
				continue;
			const std::pair id{ package->id, effect->id };
			effectEnabled[effectIndex] = std::ranges::contains(enabledEffectIds, id);
			if (id == selectedEffectId)
				selectedEffectIndex = effectIndex;
		}
		std::cout << "shader_packages=" << packages.size() << '\n';
		std::cout << "effects=" << effectEntries.size() << '\n';
		RefreshPanel(panelManager, viewer);
		return Apply(viewer);
	}

	GraphicsError::Result<void> ShaderEffectController::Apply(Viewer& viewer) const {
		std::vector<const EffectRuntimeDefinition*> activeEffects;
		for (size_t effectIndex = 0; effectIndex < effectEntries.size(); effectIndex++) {
			const auto reference = ResolveEffect(effectIndex);
			if (reference.effect && effectIndex < effectEnabled.size() && effectEnabled[effectIndex])
				activeEffects.push_back(&reference.effect->runtime);
		}
		const auto loadResult = viewer.LoadPostProcessEffects(activeEffects);
		if (!loadResult)
			return std::unexpected(loadResult.error());
		std::cout << "active_post_effects=" << activeEffects.size() << '\n';
		return {};
	}

	GraphicsError::Result<void> ShaderEffectController::ProcessPanelRequests(
		PanelManager& panelManager, Viewer& viewer) {
		size_t effectIndex = 0;
		bool enabled = false;
		if (panelManager.ConsumeSelectedShaderIndex(effectIndex, enabled)
			&& effectIndex < effectEntries.size() && effectIndex < effectEnabled.size()) {
			selectedEffectIndex = effectIndex;
			panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
			ShowEffectInformation(panelManager, selectedEffectIndex);
			const bool previousEnabled = effectEnabled[selectedEffectIndex];
			effectEnabled[selectedEffectIndex] = enabled;
			const auto applyResult = Apply(viewer);
			if (!applyResult) {
				effectEnabled[selectedEffectIndex] = previousEnabled;
				RefreshPanel(panelManager, viewer);
				return std::unexpected(applyResult.error());
			}
			ShowEffectTimeline(panelManager, selectedEffectIndex);
		}

		BuiltInShaderToggle builtInShader;
		bool builtInShaderEnabled = false;
		SceneRenderState& scene = viewer.GetSceneRenderState();
		if (!panelManager.ConsumeBuiltInShaderToggle(builtInShader, builtInShaderEnabled))
			return {};
		switch (builtInShader) {
			case BuiltInShaderToggle::Model:
				scene.modelEnabled = builtInShaderEnabled;
				break;
			case BuiltInShaderToggle::Edge:
				scene.edgeEnabled = builtInShaderEnabled;
				break;
			case BuiltInShaderToggle::GroundShadow:
				scene.groundShadowEnabled = builtInShaderEnabled;
				break;
		}
		return {};
	}
}
