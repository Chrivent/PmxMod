#include "Program/Shader/InternalShaderCatalog.h"

#include <utility>

namespace Chrivent {
	bool InternalShaderCatalog::ResolveShaderPath(const std::filesystem::path& shaderDirectory,
		const std::filesystem::path& relativePath, std::filesystem::path& shaderPath,
		std::string& error) {
		shaderPath = shaderDirectory / relativePath;
		if (std::filesystem::is_regular_file(shaderPath))
			return true;
		error = "내장 셰이더를 찾지 못했습니다: " + shaderPath.string();
		return false;
	}

	ShaderProgramDefinition InternalShaderCatalog::CreateProgram(
		std::filesystem::path shaderPath, const char* vertexEntry, const char* pixelEntry) {
		return {
			.shaderPath = std::move(shaderPath),
			.vertexEntry = vertexEntry,
			.pixelEntry = pixelEntry
		};
	}

	std::expected<SceneShaderRuntimeContract, std::string> InternalShaderCatalog::Load(
		const std::filesystem::path& shaderDirectory,
		const bool invertNdcYForTextureCoordinates) {
		std::string error;
		std::filesystem::path modelShaderPath;
		std::filesystem::path edgeShaderPath;
		std::filesystem::path groundShadowShaderPath;
		std::filesystem::path sceneInputShaderPath;
		if (!ResolveShaderPath(shaderDirectory, "model.hlsl", modelShaderPath, error)
			|| !ResolveShaderPath(shaderDirectory, "edge.hlsl", edgeShaderPath, error)
			|| !ResolveShaderPath(shaderDirectory, "ground-shadow.hlsl", groundShadowShaderPath, error)
			|| !ResolveShaderPath(shaderDirectory, "scene-input.hlsl", sceneInputShaderPath, error))
			return std::unexpected(std::move(error));
		BuiltInShaderPasses loadedBuiltInPasses;
		loadedBuiltInPasses.model = CreateProgram(std::move(modelShaderPath), "VSMain", "PSMain");
		loadedBuiltInPasses.edge = CreateProgram(std::move(edgeShaderPath), "VSMain", "PSMain");
		loadedBuiltInPasses.groundShadow = CreateProgram(
			std::move(groundShadowShaderPath), "VSMain", "PSMain");
		SceneInputShaderPasses loadedSceneInputPasses;
		loadedSceneInputPasses.depth = CreateProgram(sceneInputShaderPath, "VSDepth", "PSDepth");
		loadedSceneInputPasses.velocity = CreateProgram(
			std::move(sceneInputShaderPath), "VSVelocity",
			invertNdcYForTextureCoordinates ? "PSVelocityInvertedY" : "PSVelocity");
		return SceneShaderRuntimeContract{
			.builtIn = std::move(loadedBuiltInPasses),
			.sceneInput = std::move(loadedSceneInputPasses)
		};
	}
}
