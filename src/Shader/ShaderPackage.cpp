#include "ShaderPackage.h"

#include "../Util.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace Chrivent {
	ShaderPackageDiscovery ShaderPackageLoader::Discover(const std::filesystem::path& packagesDirectory) {
		ShaderPackageDiscovery discovery;
		std::error_code error;
		if (!std::filesystem::is_directory(packagesDirectory, error))
			return discovery;
		for (const auto& entry : std::filesystem::directory_iterator(packagesDirectory, error)) {
			if (error)
				break;
			if (!entry.is_directory())
				continue;
			const auto manifestPath = entry.path() / "package.json";
			if (!std::filesystem::is_regular_file(manifestPath))
				continue;
			ShaderPackage package;
			std::string loadError;
			if (ShaderPackageParser::Load(manifestPath, package, loadError))
				discovery.packages.emplace_back(std::move(package));
			else
				discovery.errors.emplace_back(std::move(loadError));
		}
		if (error)
			discovery.errors.emplace_back("Failed to scan shader packages: " + packagesDirectory.string());
		std::ranges::sort(discovery.packages, [](const ShaderPackage& left, const ShaderPackage& right) {
			return left.id < right.id;
		});
		return discovery;
	}
	
	bool ShaderPackageParser::ReadJsonObject(const std::filesystem::path& path, nlohmann::json& json, std::string& error) {
		std::ifstream stream(path, std::ios::binary);
		if (!stream) {
			error = "Failed to open JSON file: " + path.string();
			return false;
		}
		json = nlohmann::json::parse(stream, nullptr, false);
		if (json.is_object())
			return true;
		error = "Invalid JSON object: " + path.string();
		return false;
	}

	bool ShaderPackageParser::ReadRequiredString(const nlohmann::json& json, const char* key, std::string& value, std::string& error) {
		const auto iterator = json.find(key);
		if (iterator != json.end() && iterator->is_string()) {
			value = iterator->get<std::string>();
			if (!value.empty())
				return true;
		}
		error = "Missing or invalid string field: " + std::string(key);
		return false;
	}

	bool ShaderPackageParser::IsPathInside(const std::filesystem::path& root, const std::filesystem::path& path) {
		auto rootIterator = root.begin();
		auto pathIterator = path.begin();
		while (rootIterator != root.end()) {
			if (pathIterator == path.end() || *rootIterator != *pathIterator)
				return false;
			++rootIterator;
			++pathIterator;
		}
		return true;
	}

	bool ShaderPackageParser::ResolvePackagePath(const std::filesystem::path& packageRoot, const std::string& relativePath,
		std::filesystem::path& resolvedPath, std::string& error) {
		const std::filesystem::path requestedPath = Util::PathFromUtf8(relativePath);
		if (requestedPath.empty() || requestedPath.is_absolute()) {
			error = "Package path must be relative: " + relativePath;
			return false;
		}
		std::error_code filesystemError;
		const auto canonicalRoot = std::filesystem::weakly_canonical(packageRoot, filesystemError);
		if (filesystemError) {
			error = "Failed to resolve package root: " + packageRoot.string();
			return false;
		}
		resolvedPath = std::filesystem::weakly_canonical(canonicalRoot / requestedPath, filesystemError);
		if (filesystemError || !IsPathInside(canonicalRoot, resolvedPath)) {
			error = "Package path escapes its root: " + relativePath;
			return false;
		}
		if (std::filesystem::is_regular_file(resolvedPath, filesystemError) && !filesystemError)
			return true;
		error = "Package file does not exist: " + resolvedPath.string();
		return false;
	}

	bool ShaderPackageParser::LoadEffect(const std::filesystem::path& packageRoot, const std::filesystem::path& manifestPath,
		EffectDefinition& effect, std::string& error) {
		nlohmann::json json;
		if (!ReadJsonObject(manifestPath, json, error))
			return false;
		if (json.value("schemaVersion", 0) != 1) {
			error = "Unsupported effect schemaVersion: " + manifestPath.string();
			return false;
		}
		if (!ReadRequiredString(json, "id", effect.id, error)
			|| !ReadRequiredString(json, "name", effect.name, error)) {
			error += " in " + manifestPath.string();
			return false;
		}
		std::string type;
		if (!ReadRequiredString(json, "type", type, error)) {
			error += " in " + manifestPath.string();
			return false;
		}
		if (type == "model")
			effect.type = EffectType::Model;
		else if (type == "edge")
			effect.type = EffectType::Edge;
		else if (type == "ground_shadow")
			effect.type = EffectType::GroundShadow;
		else if (type == "post_process")
			effect.type = EffectType::PostProcess;
		else {
			error = "Unsupported effect type: " + type + " in " + manifestPath.string();
			return false;
		}
		const auto passArray = json.find("passes");
		if (passArray == json.end() || !passArray->is_array() || passArray->empty()) {
			error = "Effect requires at least one pass: " + manifestPath.string();
			return false;
		}
		effect.passes.clear();
		effect.passes.reserve(passArray->size());
		for (const auto& passJson : *passArray) {
			if (!passJson.is_object()) {
				error = "Invalid effect pass: " + manifestPath.string();
				return false;
			}
			EffectPassDefinition pass;
			std::string shaderPath;
			if (!ReadRequiredString(passJson, "name", pass.name, error)) {
				error += " in " + manifestPath.string();
				return false;
			}
			if (!ReadRequiredString(passJson, "shader", shaderPath, error)) {
				error += " in " + manifestPath.string();
				return false;
			}
			if (!ResolvePackagePath(packageRoot, shaderPath, pass.shaderPath, error))
				return false;
			pass.vertexEntry = passJson.value("vertexEntry", pass.vertexEntry);
			pass.pixelEntry = passJson.value("pixelEntry", pass.pixelEntry);
			if (pass.vertexEntry.empty() || pass.pixelEntry.empty()) {
				error = "Effect pass entry points cannot be empty: " + manifestPath.string();
				return false;
			}
			effect.passes.emplace_back(std::move(pass));
		}
		return true;
	}

	bool ShaderPackageParser::Load(
		const std::filesystem::path& manifestPath, ShaderPackage& package, std::string& error) {
		nlohmann::json json;
		if (!ReadJsonObject(manifestPath, json, error))
			return false;
		if (json.value("schemaVersion", 0) != 1) {
			error = "Unsupported package schemaVersion: " + manifestPath.string();
			return false;
		}
		ShaderPackage loaded;
		if (!ReadRequiredString(json, "id", loaded.id, error)
			|| !ReadRequiredString(json, "name", loaded.name, error)
			|| !ReadRequiredString(json, "version", loaded.version, error)) {
			error += " in " + manifestPath.string();
			return false;
		}
		loaded.author = json.value("author", std::string{});
		loaded.rootPath = std::filesystem::weakly_canonical(manifestPath.parent_path());
		const auto effectArray = json.find("effects");
		if (effectArray == json.end() || !effectArray->is_array() || effectArray->empty()) {
			error = "Package requires at least one effect: " + manifestPath.string();
			return false;
		}
		loaded.effects.reserve(effectArray->size());
		for (const auto& effectPathJson : *effectArray) {
			if (!effectPathJson.is_string()) {
				error = "Package effect path must be a string: " + manifestPath.string();
				return false;
			}
			std::filesystem::path effectManifestPath;
			if (!ResolvePackagePath(
				loaded.rootPath, effectPathJson.get<std::string>(), effectManifestPath, error))
				return false;
			EffectDefinition effect;
			if (!LoadEffect(loaded.rootPath, effectManifestPath, effect, error))
				return false;
			loaded.effects.emplace_back(std::move(effect));
		}
		package = std::move(loaded);
		return true;
	}
}
