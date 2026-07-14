#include "Viewer/Shader/ShaderPackage.h"

#include "Util.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace Chrivent {
	ShaderPackageDiscovery ShaderPackageLoader::Discover(const std::filesystem::path& packagesDirectory) {
		ShaderPackageDiscovery discovery;
		std::unordered_set<std::string> packageIds;
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
			if (!ShaderPackageParser::Load(manifestPath, package, loadError))
				discovery.errors.emplace_back(std::move(loadError));
			else if (!packageIds.emplace(package.id).second)
				discovery.errors.emplace_back("Duplicate shader package id: " + package.id);
			else
				discovery.packages.emplace_back(std::move(package));
		}
		if (error)
			discovery.errors.emplace_back("Failed to scan shader packages: " + packagesDirectory.string());
		std::ranges::sort(discovery.packages, [](const ShaderPackage& left, const ShaderPackage& right) {
			return left.id < right.id;
		});
		return discovery;
	}

	bool ShaderPackageParser::ReadJsonObject(
		const std::filesystem::path& path, nlohmann::json& json, std::string& error) {
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

	bool ShaderPackageParser::ReadRequiredString(
		const nlohmann::json& json, const char* key, std::string& value, std::string& error) {
		const auto iterator = json.find(key);
		if (iterator != json.end() && iterator->is_string()) {
			value = iterator->get<std::string>();
			if (!value.empty())
				return true;
		}
		error = "Missing or invalid string field: " + std::string(key);
		return false;
	}

	bool ShaderPackageParser::IsPathInside(
		const std::filesystem::path& root, const std::filesystem::path& path) {
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

	bool ShaderPackageParser::ResolvePackagePath(const std::filesystem::path& packageRoot,
		const std::string& relativePath, std::filesystem::path& resolvedPath, std::string& error) {
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

	bool ShaderPackageParser::LoadPass(const std::filesystem::path& packageRoot,
		const std::filesystem::path& manifestPath, const nlohmann::json& json, const bool postProcess,
		EffectPassDefinition& pass, std::string& error) {
		if (!json.is_object()) {
			error = "Invalid effect pass: " + manifestPath.string();
			return false;
		}
		std::string shaderPath;
		if (!ReadRequiredString(json, "name", pass.name, error)
			|| !ReadRequiredString(json, "shader", shaderPath, error)) {
			error += " in " + manifestPath.string();
			return false;
		}
		if (!ResolvePackagePath(packageRoot, shaderPath, pass.shaderPath, error))
			return false;
		pass.vertexEntry = json.value("vertexEntry", pass.vertexEntry);
		pass.pixelEntry = json.value("pixelEntry", pass.pixelEntry);
		if (pass.vertexEntry.empty() || pass.pixelEntry.empty()) {
			error = "Effect pass entry points cannot be empty: " + manifestPath.string();
			return false;
		}
		if (!postProcess)
			return true;
		const auto reads = json.find("reads");
		if (reads == json.end() || !reads->is_array()) {
			error = "Post-process pass reads must be an array: " + manifestPath.string();
			return false;
		}
		std::unordered_set<uint32_t> usedSlots;
		for (const auto& inputJson : *reads) {
			if (!inputJson.is_object()) {
				error = "Invalid post-process pass input: " + manifestPath.string();
				return false;
			}
			const auto slot = inputJson.find("slot");
			EffectPassInputDefinition input;
			if (slot == inputJson.end() || !slot->is_number_unsigned()) {
				error = "Post-process input slot must be an unsigned integer: " + manifestPath.string();
				return false;
			}
			input.slot = slot->get<uint32_t>();
			if (input.slot >= PostProcessInputLayout::maxTextureCount || !usedSlots.emplace(input.slot).second
				|| !ReadRequiredString(inputJson, "resource", input.resource, error)) {
				error = "Invalid or duplicate post-process input slot: " + manifestPath.string();
				return false;
			}
			pass.inputs.emplace_back(std::move(input));
		}
		if (ReadRequiredString(json, "output", pass.output, error))
			return true;
		error += " in " + manifestPath.string();
		return false;
	}

	bool ShaderPackageParser::LoadResources(const nlohmann::json& json,
		const std::filesystem::path& manifestPath, std::vector<EffectResourceDefinition>& resources,
		std::string& error) {
		resources.clear();
		const auto resourceArray = json.find("resources");
		if (resourceArray == json.end())
			return true;
		if (!resourceArray->is_array()) {
			error = "Effect resources must be an array: " + manifestPath.string();
			return false;
		}
		std::unordered_set<std::string> names;
		for (const auto& resourceJson : *resourceArray) {
			EffectResourceDefinition resource;
			std::string lifetime;
			std::string format;
			if (!resourceJson.is_object()
				|| !ReadRequiredString(resourceJson, "name", resource.name, error)
				|| !ReadRequiredString(resourceJson, "lifetime", lifetime, error)
				|| !ReadRequiredString(resourceJson, "format", format, error)
				|| resource.name.contains('.') || !names.emplace(resource.name).second) {
				error = "Invalid or duplicate effect resource: " + manifestPath.string();
				return false;
			}
			if (lifetime == "transient")
				resource.lifetime = EffectResourceLifetime::Transient;
			else if (lifetime == "history")
				resource.lifetime = EffectResourceLifetime::History;
			else {
				error = "Unsupported effect resource lifetime: " + lifetime + " in " + manifestPath.string();
				return false;
			}
			if (format == "rgba8_unorm")
				resource.format = EffectTextureFormat::Rgba8Unorm;
			else if (format == "rgba16_float")
				resource.format = EffectTextureFormat::Rgba16Float;
			else if (format == "rgba32_float")
				resource.format = EffectTextureFormat::Rgba32Float;
			else {
				error = "Unsupported effect resource format: " + format + " in " + manifestPath.string();
				return false;
			}
			const auto size = resourceJson.find("size");
			const auto resolution = resourceJson.find("resolution");
			if (size != resourceJson.end() && resolution != resourceJson.end()) {
				error = "Effect resource cannot declare both size and resolution: " + manifestPath.string();
				return false;
			}
			if (size != resourceJson.end()) {
				if (!size->is_object() || !size->contains("width") || !size->contains("height")
					|| !(*size)["width"].is_number_unsigned() || !(*size)["height"].is_number_unsigned()) {
					error = "Invalid fixed effect resource size: " + manifestPath.string();
					return false;
				}
				resource.resolution = EffectPassResolution::Fixed;
				resource.width = (*size)["width"].get<uint32_t>();
				resource.height = (*size)["height"].get<uint32_t>();
				if (resource.width == 0 || resource.height == 0) {
					error = "Fixed effect resource size cannot be zero: " + manifestPath.string();
					return false;
				}
			} else {
				const std::string value = resolution == resourceJson.end()
					? "full" : resolution->is_string() ? resolution->get<std::string>() : std::string{};
				if (value == "full")
					resource.resolution = EffectPassResolution::Full;
				else if (value == "half")
					resource.resolution = EffectPassResolution::Half;
				else if (value == "quarter")
					resource.resolution = EffectPassResolution::Quarter;
				else if (value == "eighth")
					resource.resolution = EffectPassResolution::Eighth;
				else {
					error = "Unsupported effect resource resolution: " + value + " in " + manifestPath.string();
					return false;
				}
			}
			resources.emplace_back(std::move(resource));
		}
		return true;
	}

	bool ShaderPackageParser::LoadParameters(const nlohmann::json& json,
		const std::filesystem::path& manifestPath, std::vector<EffectParameterDefinition>& parameters,
		std::string& error) {
		parameters.clear();
		const auto parameterArray = json.find("parameters");
		if (parameterArray == json.end())
			return true;
		if (!parameterArray->is_array()) {
			error = "Effect parameters must be an array: " + manifestPath.string();
			return false;
		}
		std::unordered_set<std::string> ids;
		std::unordered_set<uint32_t> slots;
		for (const auto& parameterJson : *parameterArray) {
			if (!parameterJson.is_object()) {
				error = "Invalid effect parameter: " + manifestPath.string();
				return false;
			}
			EffectParameterDefinition parameter;
			const auto slot = parameterJson.find("slot");
			const auto defaultValue = parameterJson.find("default");
			const auto minimumValue = parameterJson.find("min");
			const auto maximumValue = parameterJson.find("max");
			if (!ReadRequiredString(parameterJson, "id", parameter.id, error)
				|| !ReadRequiredString(parameterJson, "name", parameter.name, error)
				|| slot == parameterJson.end() || !slot->is_number_unsigned()
				|| defaultValue == parameterJson.end() || !defaultValue->is_number()
				|| minimumValue == parameterJson.end() || !minimumValue->is_number()
				|| maximumValue == parameterJson.end() || !maximumValue->is_number()) {
				error = "Invalid effect parameter: " + manifestPath.string();
				return false;
			}
			parameter.slot = slot->get<uint32_t>();
			parameter.defaultValue = defaultValue->get<float>();
			parameter.minimumValue = minimumValue->get<float>();
			parameter.maximumValue = maximumValue->get<float>();
			if (parameter.slot >= PostProcessInputLayout::maxParameterCount
				|| !ids.emplace(parameter.id).second || !slots.emplace(parameter.slot).second
				|| !std::isfinite(parameter.defaultValue) || !std::isfinite(parameter.minimumValue)
				|| !std::isfinite(parameter.maximumValue)
				|| parameter.minimumValue > parameter.maximumValue
				|| parameter.defaultValue < parameter.minimumValue
				|| parameter.defaultValue > parameter.maximumValue) {
				error = "Invalid or duplicate effect parameter slot: " + manifestPath.string();
				return false;
			}
			parameters.emplace_back(std::move(parameter));
		}
		return true;
	}

	bool ShaderPackageParser::LoadEffect(const std::filesystem::path& packageRoot,
		const std::filesystem::path& manifestPath, EffectDefinition& effect, std::string& error) {
		nlohmann::json json;
		if (!ReadJsonObject(manifestPath, json, error))
			return false;
		if (json.value("schemaVersion", 0) != schemaVersion) {
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
		const bool postProcess = effect.type == EffectType::PostProcess;
		effect.inputs.clear();
		if (postProcess) {
			const auto inputs = json.find("inputs");
			if (inputs == json.end() || !inputs->is_array()) {
				error = "Post-process effect inputs must be an array: " + manifestPath.string();
				return false;
			}
			std::unordered_set<std::string> inputNames;
			for (const auto& input : *inputs) {
				if (!input.is_string()) {
					error = "Post-process effect input must be a string: " + manifestPath.string();
					return false;
				}
				const std::string value = input.get<std::string>();
				if ((value != "scene_color" && value != "scene_depth" && value != "scene_velocity")
					|| !inputNames.emplace(value).second) {
					error = "Unsupported or duplicate engine input: " + value + " in " + manifestPath.string();
					return false;
				}
				effect.inputs.emplace_back(value);
			}
		}
		if (postProcess) {
			if (!LoadParameters(json, manifestPath, effect.parameters, error)
				|| !LoadResources(json, manifestPath, effect.resources, error))
				return false;
		} else {
			effect.parameters.clear();
			effect.resources.clear();
		}
		const auto passArray = json.find("passes");
		if (passArray == json.end() || !passArray->is_array() || passArray->empty()) {
			error = "Effect requires at least one pass: " + manifestPath.string();
			return false;
		}
		effect.passes.clear();
		std::unordered_set<std::string> passNames;
		for (const auto& passJson : *passArray) {
			EffectPassDefinition pass;
			if (!LoadPass(packageRoot, manifestPath, passJson, postProcess, pass, error))
				return false;
			if (!passNames.emplace(pass.name).second) {
				error = "Duplicate effect pass name: " + pass.name + " in " + manifestPath.string();
				return false;
			}
			effect.passes.emplace_back(std::move(pass));
		}
		if (!postProcess)
			return true;
		std::unordered_map<std::string, EffectResourceLifetime> resources;
		for (const auto& resource : effect.resources) {
			if (resource.name == "effect_input" || resource.name == "effect_output"
				|| resource.name == "scene_color" || resource.name == "scene_depth"
				|| resource.name == "scene_velocity") {
				error = "Effect resource uses a reserved name: " + resource.name + " in " + manifestPath.string();
				return false;
			}
			resources.emplace(resource.name, resource.lifetime);
		}
		std::unordered_set<std::string> writtenTransientResources;
		for (size_t passIndex = 0; passIndex < effect.passes.size(); passIndex++) {
			const EffectPassDefinition& pass = effect.passes[passIndex];
			for (const auto& [slot, resource] : pass.inputs) {
				if (resource == "effect_input")
					continue;
				if (resource == "scene_color" || resource == "scene_depth" || resource == "scene_velocity") {
					if (std::ranges::find(effect.inputs, resource) == effect.inputs.end()) {
						error = "Pass uses an undeclared engine input: " + resource + " in " + manifestPath.string();
						return false;
					}
					continue;
				}
				std::string resourceName = resource;
				const bool historyRead = resourceName.ends_with(".read");
				if (historyRead)
					resourceName.resize(resourceName.size() - 5);
				const auto findResource = resources.find(resourceName);
				if (findResource == resources.end() || historyRead != (findResource->second == EffectResourceLifetime::History)
					|| (!historyRead && !writtenTransientResources.contains(resourceName))) {
					error = "Invalid or uninitialized pass input resource: " + resource + " in " + manifestPath.string();
					return false;
				}
			}
			if (pass.output == "effect_output") {
				if (passIndex + 1 != effect.passes.size()) {
					error = "Only the final effect pass can write effect_output: " + manifestPath.string();
					return false;
				}
				continue;
			}
			std::string outputName = pass.output;
			const bool historyWrite = outputName.ends_with(".write");
			if (historyWrite)
				outputName.resize(outputName.size() - 6);
			const auto resource = resources.find(outputName);
			if (resource == resources.end() || historyWrite != (resource->second == EffectResourceLifetime::History)) {
				error = "Invalid pass output resource: " + pass.output + " in " + manifestPath.string();
				return false;
			}
			if (!historyWrite && std::ranges::any_of(pass.inputs, [&outputName](const EffectPassInputDefinition& input) {
				return input.resource == outputName;
			})) {
				error = "A transient resource cannot be read and written by the same pass: "
					+ outputName + " in " + manifestPath.string();
				return false;
			}
			if (!historyWrite)
				writtenTransientResources.emplace(outputName);
		}
		if (effect.passes.back().output == "effect_output")
			return true;
		error = "The final post-process pass must write effect_output: " + manifestPath.string();
		return false;
	}

	bool ShaderPackageParser::Load(
		const std::filesystem::path& manifestPath, ShaderPackage& package, std::string& error) {
		nlohmann::json json;
		if (!ReadJsonObject(manifestPath, json, error))
			return false;
		if (json.value("schemaVersion", 0) != schemaVersion) {
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
		std::unordered_set<std::string> effectIds;
		for (const auto& effectPathJson : *effectArray) {
			if (!effectPathJson.is_string()) {
				error = "Package effect path must be a string: " + manifestPath.string();
				return false;
			}
			std::filesystem::path effectManifestPath;
			if (!ResolvePackagePath(loaded.rootPath, effectPathJson.get<std::string>(), effectManifestPath, error))
				return false;
			EffectDefinition effect;
			if (!LoadEffect(loaded.rootPath, effectManifestPath, effect, error))
				return false;
			if (!effectIds.emplace(effect.id).second) {
				error = "Duplicate package effect id: " + effect.id + " in " + manifestPath.string();
				return false;
			}
			loaded.effects.emplace_back(std::move(effect));
		}
		package = std::move(loaded);
		return true;
	}

	bool BuiltInShaderContract::Load(const std::filesystem::path& manifestPath,
		BuiltInShaderPasses& passes, std::string& error) {
		ShaderPackage package;
		if (!ShaderPackageParser::Load(manifestPath, package, error))
			return false;
		BuiltInShaderPasses loaded;
		if (!ResolvePass(package, EffectType::Model, "model", loaded.model, error)
			|| !ResolvePass(package, EffectType::Edge, "edge", loaded.edge, error)
			|| !ResolvePass(package, EffectType::GroundShadow, "ground shadow", loaded.groundShadow, error))
			return false;
		passes = std::move(loaded);
		return true;
	}

	bool BuiltInShaderContract::ResolvePass(const ShaderPackage& package, const EffectType type,
		const char* role, EffectPassDefinition& pass, std::string& error) {
		const EffectDefinition* match = nullptr;
		for (const auto& effect : package.effects) {
			if (effect.type != type)
				continue;
			if (match != nullptr) {
				error = "Built-in shader role must be unique: " + std::string(role);
				return false;
			}
			match = &effect;
		}
		if (match == nullptr) {
			error = "Built-in shader role is missing: " + std::string(role);
			return false;
		}
		if (match->passes.size() != 1) {
			error = "Built-in shader role requires exactly one pass: " + std::string(role);
			return false;
		}
		pass = match->passes.front();
		return true;
	}
}
