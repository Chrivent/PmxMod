#include "Program/Shader/ShaderPackage.h"

#include "Util.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Chrivent {
	std::string ShaderPackageError::Format() const {
		return message;
	}

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
			auto loadResult = ShaderPackageParser::Load(manifestPath);
			if (!loadResult) {
				discovery.errors.emplace_back(std::move(loadResult.error()));
				continue;
			}
			ShaderPackage package = std::move(*loadResult);
			if (!packageIds.emplace(package.id).second) {
				discovery.errors.emplace_back(ShaderPackageError{
					.code = ShaderPackageErrorCode::DuplicatePackageId,
					.manifestPath = manifestPath,
					.message = "Duplicate shader package id: " + package.id
				});
				continue;
			}
			discovery.packages.emplace_back(std::move(package));
		}
		if (error)
			discovery.errors.emplace_back(ShaderPackageError{
				.code = ShaderPackageErrorCode::ScanFailed,
				.manifestPath = packagesDirectory,
				.message = "Failed to scan shader packages: " + packagesDirectory.string()
			});
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
		const std::filesystem::path& manifestPath, const nlohmann::json& json,
		ParsedEffectPass& pass, std::string& error) {
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
		if (!ResolvePackagePath(packageRoot, shaderPath, pass.program.shaderPath, error))
			return false;
		pass.program.vertexEntry = json.value("vertexEntry", pass.program.vertexEntry);
		pass.program.pixelEntry = json.value("pixelEntry", pass.program.pixelEntry);
		if (pass.program.vertexEntry.empty() || pass.program.pixelEntry.empty()) {
			error = "Effect pass entry points cannot be empty: " + manifestPath.string();
			return false;
		}
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
			ParsedEffectPassInput input;
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
		const std::filesystem::path& manifestPath, std::vector<ParsedEffectResource>& resources,
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
			ParsedEffectResource resource;
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
				resource.definition.lifetime = EffectResourceLifetime::Transient;
			else if (lifetime == "history")
				resource.definition.lifetime = EffectResourceLifetime::History;
			else {
				error = "Unsupported effect resource lifetime: " + lifetime + " in " + manifestPath.string();
				return false;
			}
			if (format == "rgba8_unorm")
				resource.definition.format = EffectTextureFormat::Rgba8Unorm;
			else if (format == "rgba16_float")
				resource.definition.format = EffectTextureFormat::Rgba16Float;
			else if (format == "rgba32_float")
				resource.definition.format = EffectTextureFormat::Rgba32Float;
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
				resource.definition.resolution = EffectPassResolution::Fixed;
				resource.definition.width = (*size)["width"].get<uint32_t>();
				resource.definition.height = (*size)["height"].get<uint32_t>();
				if (resource.definition.width == 0 || resource.definition.height == 0) {
					error = "Fixed effect resource size cannot be zero: " + manifestPath.string();
					return false;
				}
			} else {
				const std::string value = resolution == resourceJson.end()
					? "full" : resolution->is_string() ? resolution->get<std::string>() : std::string{};
				if (value == "full")
					resource.definition.resolution = EffectPassResolution::Full;
				else if (value == "half")
					resource.definition.resolution = EffectPassResolution::Half;
				else if (value == "quarter")
					resource.definition.resolution = EffectPassResolution::Quarter;
				else if (value == "eighth")
					resource.definition.resolution = EffectPassResolution::Eighth;
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
		nlohmann::json effectJson;
		if (!ReadJsonObject(manifestPath, effectJson, error))
			return false;
		if (effectJson.value("schemaVersion", 0) != schemaVersion) {
			error = "Unsupported effect schemaVersion: " + manifestPath.string();
			return false;
		}
		if (!ReadRequiredString(effectJson, "id", effect.id, error)
			|| !ReadRequiredString(effectJson, "name", effect.name, error)) {
			error += " in " + manifestPath.string();
			return false;
		}
		auto& [parameters, resources, passes] = effect.runtime;
		std::string typeName;
		if (!ReadRequiredString(effectJson, "type", typeName, error)) {
			error += " in " + manifestPath.string();
			return false;
		}
		if (typeName != "post_process") {
			error = "Unsupported effect type: " + typeName + " in " + manifestPath.string();
			return false;
		}
		const auto inputValues = effectJson.find("inputs");
		if (inputValues == effectJson.end() || !inputValues->is_array()) {
			error = "Post-process effect inputs must be an array: " + manifestPath.string();
			return false;
		}
		std::unordered_set<std::string> uniqueInputNames;
		for (const auto& inputJson : *inputValues) {
			if (!inputJson.is_string()) {
				error = "Post-process effect input must be a string: " + manifestPath.string();
				return false;
			}
			const std::string inputName = inputJson.get<std::string>();
			if ((inputName != "scene_color" && inputName != "scene_depth" && inputName != "scene_velocity")
				|| !uniqueInputNames.emplace(inputName).second) {
				error = "Unsupported or duplicate engine input: " + inputName + " in " + manifestPath.string();
				return false;
			}
		}
		std::vector<ParsedEffectResource> parsedResources;
		if (!LoadParameters(effectJson, manifestPath, parameters, error)
			|| !LoadResources(effectJson, manifestPath, parsedResources, error))
			return false;
		const auto passValues = effectJson.find("passes");
		if (passValues == effectJson.end() || !passValues->is_array() || passValues->empty()) {
			error = "Effect requires at least one pass: " + manifestPath.string();
			return false;
		}
		std::vector<ParsedEffectPass> parsedPasses;
		std::unordered_set<std::string> uniquePassNames;
		for (const auto& passJson : *passValues) {
			ParsedEffectPass pass;
			if (!LoadPass(packageRoot, manifestPath, passJson, pass, error))
				return false;
			if (!uniquePassNames.emplace(pass.name).second) {
				error = "Duplicate effect pass name: " + pass.name + " in " + manifestPath.string();
				return false;
			}
			parsedPasses.emplace_back(std::move(pass));
		}
		std::unordered_map<std::string, ParsedResourceReference> resourceDefinitions;
		resources.clear();
		resources.reserve(parsedResources.size());
		for (auto& resource : parsedResources) {
			if (resource.name == "effect_input" || resource.name == "effect_output"
				|| resource.name == "scene_color" || resource.name == "scene_depth"
				|| resource.name == "scene_velocity") {
				error = "Effect resource uses a reserved name: "
					+ resource.name + " in " + manifestPath.string();
				return false;
			}
			const size_t resourceIndex = resources.size();
			resourceDefinitions.emplace(resource.name, ParsedResourceReference{
				.index = resourceIndex,
				.lifetime = resource.definition.lifetime
			});
			resources.emplace_back(std::move(resource.definition));
		}
		passes.clear();
		passes.reserve(parsedPasses.size());
		std::unordered_set<std::string> initializedTransientResources;
		for (size_t passIndex = 0; passIndex < parsedPasses.size(); passIndex++) {
			ParsedEffectPass& parsedPass = parsedPasses[passIndex];
			EffectPassDefinition pass{ .program = std::move(parsedPass.program) };
			pass.inputs.reserve(parsedPass.inputs.size());
			for (const auto& [slot, resource] : parsedPass.inputs) {
				EffectPassInputDefinition input{ .slot = slot };
				if (resource == "effect_input") {
					input.kind = EffectPassInputKind::EffectInput;
					pass.inputs.emplace_back(input);
					continue;
				}
				if (resource == "scene_color" || resource == "scene_depth" || resource == "scene_velocity") {
					if (!uniqueInputNames.contains(resource)) {
						error = "Pass uses an undeclared engine input: " + resource + " in " + manifestPath.string();
						return false;
					}
					if (resource == "scene_color")
						input.kind = EffectPassInputKind::SceneColor;
					else if (resource == "scene_depth")
						input.kind = EffectPassInputKind::SceneDepth;
					else
						input.kind = EffectPassInputKind::SceneVelocity;
					pass.inputs.emplace_back(input);
					continue;
				}
				std::string inputResourceName = resource;
				const bool readsHistoryResource = inputResourceName.ends_with(".read");
				if (readsHistoryResource)
					inputResourceName.resize(inputResourceName.size() - 5);
				const auto resourceDefinition = resourceDefinitions.find(inputResourceName);
				if (resourceDefinition == resourceDefinitions.end()
					|| readsHistoryResource !=
						(resourceDefinition->second.lifetime == EffectResourceLifetime::History)
					|| (!readsHistoryResource && !initializedTransientResources.contains(inputResourceName))) {
					error = "Invalid or uninitialized pass input resource: " + resource + " in " + manifestPath.string();
					return false;
				}
				input.kind = EffectPassInputKind::Resource;
				input.resourceIndex = resourceDefinition->second.index;
				pass.inputs.emplace_back(input);
			}
			if (parsedPass.output == "effect_output") {
				if (passIndex + 1 != parsedPasses.size()) {
					error = "Only the final effect pass can write effect_output: " + manifestPath.string();
					return false;
				}
				passes.emplace_back(std::move(pass));
				continue;
			}
			std::string outputResourceName = parsedPass.output;
			const bool writesHistoryResource = outputResourceName.ends_with(".write");
			if (writesHistoryResource)
				outputResourceName.resize(outputResourceName.size() - 6);
			const auto outputResourceDefinition = resourceDefinitions.find(outputResourceName);
			if (outputResourceDefinition == resourceDefinitions.end()
				|| writesHistoryResource !=
					(outputResourceDefinition->second.lifetime == EffectResourceLifetime::History)) {
				error = "Invalid pass output resource: " + parsedPass.output + " in " + manifestPath.string();
				return false;
			}
			if (!writesHistoryResource && std::ranges::any_of(parsedPass.inputs,
				[&outputResourceName](const ParsedEffectPassInput& input) {
					return input.resource == outputResourceName;
				})) {
				error = "A transient resource cannot be read and written by the same pass: "
					+ outputResourceName + " in " + manifestPath.string();
				return false;
			}
			if (!writesHistoryResource)
				initializedTransientResources.emplace(outputResourceName);
			pass.output.kind = EffectPassOutputKind::Resource;
			pass.output.resourceIndex = outputResourceDefinition->second.index;
			passes.emplace_back(std::move(pass));
		}
		if (parsedPasses.back().output == "effect_output")
			return true;
		error = "The final post-process pass must write effect_output: " + manifestPath.string();
		return false;
	}

	bool ShaderPackageParser::LoadPackage(const std::filesystem::path& manifestPath,
		ShaderPackage& package, std::string& error) {
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

	std::expected<ShaderPackage, ShaderPackageError> ShaderPackageParser::Load(const std::filesystem::path& manifestPath) {
		ShaderPackage package;
		std::string error;
		if (!LoadPackage(manifestPath, package, error)) {
			return std::unexpected(ShaderPackageError{
				.code = ShaderPackageErrorCode::InvalidPackage,
				.manifestPath = manifestPath,
				.message = std::move(error)
			});
		}
		return package;
	}
}
