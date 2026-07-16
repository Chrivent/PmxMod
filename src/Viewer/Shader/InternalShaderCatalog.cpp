#include "Viewer/Shader/InternalShaderCatalog.h"

#include <utility>

namespace Chrivent {
	bool InternalShaderCatalog::ResolveShaderPath(const std::filesystem::path& shaderDirectory,
		const std::filesystem::path& relativePath, std::filesystem::path& shaderPath,
		std::string& error) {
		shaderPath = shaderDirectory / relativePath;
		if (std::filesystem::is_regular_file(shaderPath))
			return true;
		error = "Failed to find the internal shader: " + shaderPath.string();
		return false;
	}

	EffectPassDefinition InternalShaderCatalog::CreatePass(std::string name,
		std::filesystem::path shaderPath, const char* vertexEntry, const char* pixelEntry) {
		EffectPassDefinition pass;
		pass.name = std::move(name);
		pass.shaderPath = std::move(shaderPath);
		pass.vertexEntry = vertexEntry;
		pass.pixelEntry = pixelEntry;
		return pass;
	}

	bool InternalShaderCatalog::Load(const std::filesystem::path& shaderDirectory,
		BuiltInShaderPasses& builtInPasses, SceneInputShaderPasses& sceneInputPasses,
		std::string& error) {
		std::filesystem::path modelShaderPath;
		std::filesystem::path edgeShaderPath;
		std::filesystem::path groundShadowShaderPath;
		std::filesystem::path sceneInputShaderPath;
		if (!ResolveShaderPath(shaderDirectory, "model.hlsl", modelShaderPath, error)
			|| !ResolveShaderPath(shaderDirectory, "edge.hlsl", edgeShaderPath, error)
			|| !ResolveShaderPath(shaderDirectory, "ground-shadow.hlsl", groundShadowShaderPath, error)
			|| !ResolveShaderPath(shaderDirectory, "scene-input.hlsl", sceneInputShaderPath, error))
			return false;
		BuiltInShaderPasses loadedBuiltInPasses;
		loadedBuiltInPasses.model = CreatePass("model", std::move(modelShaderPath), "VSMain", "PSMain");
		loadedBuiltInPasses.edge = CreatePass("edge", std::move(edgeShaderPath), "VSMain", "PSMain");
		loadedBuiltInPasses.groundShadow = CreatePass(
			"ground_shadow", std::move(groundShadowShaderPath), "VSMain", "PSMain");
		SceneInputShaderPasses loadedSceneInputPasses;
		loadedSceneInputPasses.depth = CreatePass(
			"scene_depth", sceneInputShaderPath, "VSDepth", "PSDepth");
		loadedSceneInputPasses.velocity = CreatePass(
			"scene_velocity", sceneInputShaderPath, "VSVelocity", "PSVelocity");
		loadedSceneInputPasses.velocityInvertedY = CreatePass(
			"scene_velocity_inverted_y", std::move(sceneInputShaderPath),
			"VSVelocity", "PSVelocityInvertedY");
		builtInPasses = std::move(loadedBuiltInPasses);
		sceneInputPasses = std::move(loadedSceneInputPasses);
		error.clear();
		return true;
	}
}
