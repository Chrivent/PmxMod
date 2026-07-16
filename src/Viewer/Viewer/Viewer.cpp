#include "Viewer/Viewer/Viewer.h"

#include "Viewer/Instance/Instance.h"
#include "Viewer/PostProcess/PostProcess.h"
#include "Viewer/Shader/ShaderPackage.h"

#include <Windows.h>
#include <iostream>
#include <utility>

namespace Chrivent {
	void Viewer::ResetPostProcessFrameHistory() {
		postProcessTemporalState.historyResetPending = true;
		postProcessTemporalState.frameData.historyReset = 1.0f;
	}

	void Viewer::CommitPostProcessFrameHistory() {
		postProcessTemporalState.previousViewMatrix = viewMat;
		postProcessTemporalState.previousProjectionMatrix = projMat;
		postProcessTemporalState.historyResetPending = false;
		postProcessTemporalState.frameData.historyReset = 0.0f;
	}

	void Viewer::UpdatePostProcessFrameData(PostProcessFrameData frameData) {
		postProcessTemporalState.frameData = std::move(frameData);
	}

	bool Viewer::InitializeShaderResources() {
		InitializeDirectories();
		return LoadSceneInputShaderContract() && LoadBuiltInShaderContract();
	}

	bool Viewer::LoadPostProcessEffects(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!WaitIdle() || !LoadPostProcessEffectsCore(effects))
			return false;
		ResetPostProcessFrameHistory();
		return true;
	}

	PostProcessSceneInputBeginResult Viewer::BeginPostProcessSceneInputPass() {
		const PostProcess& postProcess = ResolvePostProcess();
		if (!postProcess.RequiresDepth() && !postProcess.RequiresVelocity())
			return PostProcessSceneInputBeginResult::NotRequired;
		return BeginPostProcessSceneInputPassCore()
			? PostProcessSceneInputBeginResult::Ready : PostProcessSceneInputBeginResult::Failed;
	}

	std::unique_ptr<Instance> Viewer::CreateInstance(std::shared_ptr<Model> model,
		std::unique_ptr<Animation> animation, const float scale) {
		auto instance = CreateInstanceCore();
		if (!instance || !instance->Initialize(std::move(model), std::move(animation), scale))
			return nullptr;
		return instance;
	}

	void Viewer::ResetPostProcessHistory() {
		ResolvePostProcess().ResetHistory();
		ResetPostProcessFrameHistory();
	}

	bool Viewer::RequiresPostProcessVelocity() const {
		return ResolvePostProcess().RequiresVelocity();
	}

	bool Viewer::LoadSceneInputShaderContract() {
		const std::filesystem::path shaderPath = internalShaderDir / "scene-input.hlsl";
		if (!std::filesystem::is_regular_file(shaderPath)) {
			std::cerr << "Failed to find the internal scene input shader: " << shaderPath << '\n';
			return false;
		}
		sceneInputShaderPasses = {};
		sceneInputShaderPasses.depth.shaderPath = shaderPath;
		sceneInputShaderPasses.depth.vertexEntry = "VSDepth";
		sceneInputShaderPasses.depth.pixelEntry = "PSDepth";
		sceneInputShaderPasses.velocity.shaderPath = shaderPath;
		sceneInputShaderPasses.velocity.vertexEntry = "VSVelocity";
		sceneInputShaderPasses.velocity.pixelEntry = "PSVelocity";
		sceneInputShaderPasses.velocityInvertedY = sceneInputShaderPasses.velocity;
		sceneInputShaderPasses.velocityInvertedY.pixelEntry = "PSVelocityInvertedY";
		return true;
	}

	bool Viewer::LoadBuiltInShaderContract() {
		std::string error;
		if (BuiltInShaderContract::Load(ResolveShaderPackagesDirectory() / "pmxmod-default" / "package.json",
			builtInShaderPasses, error))
			return true;
		std::cerr << error << '\n';
		return false;
	}

	void Viewer::InitializeDirectories() {
        std::vector<wchar_t> buf(MAX_PATH);
        while (true) {
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(), buf.size());
            if (n < buf.size() - 1) {
                resourceDir = std::filesystem::path(std::wstring(buf.data(), n));
                break;
            }
            buf.resize(buf.size() * 2);
        }
        resourceDir = resourceDir.parent_path() / "resource";
		internalShaderDir = resourceDir / "internal" / "shaders";
        pmxDir = resourceDir / "mmd";
    }

}
