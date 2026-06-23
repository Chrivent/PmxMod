#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Chrivent {
	enum class EffectType {
		Model,
		Edge,
		GroundShadow,
		PostProcess
	};

	struct EffectPassDefinition {
		std::string name;
		std::filesystem::path shaderPath;
		std::filesystem::path vertexShaderPath;
		std::filesystem::path fragmentShaderPath;
		std::string vertexEntry = "VSMain";
		std::string pixelEntry = "PSMain";
	};

	struct EffectDefinition {
		std::string id;
		std::string name;
		EffectType type = EffectType::PostProcess;
		std::vector<EffectPassDefinition> passes;
	};

	struct ShaderPackage {
		std::string id;
		std::string name;
		std::string version;
		std::string author;
		std::filesystem::path rootPath;
		std::vector<EffectDefinition> effects;
	};

	struct ShaderPackageDiscovery {
		std::vector<ShaderPackage> packages;
		std::vector<std::string> errors;
	};

	class ShaderPackageLoader {
	public:
		// 지정한 디렉터리 바로 아래에서 셰이더 패키지를 검색한다.
		static ShaderPackageDiscovery Discover(const std::filesystem::path& packagesDirectory);
	};
	
	class ShaderPackageParser {
		// JSON 파일을 읽고 최상위 객체인지 확인한다.
		static bool ReadJsonObject(const std::filesystem::path& path, nlohmann::json& json, std::string& error);
		// 필수 문자열 필드를 읽는다.
		static bool ReadRequiredString(const nlohmann::json& json, const char* key, std::string& value, std::string& error);
		// 경로가 지정한 루트 내부인지 확인한다.
		static bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& path);
		// 패키지 내부의 상대 경로를 실제 파일 경로로 변환한다.
		static bool ResolvePackagePath(const std::filesystem::path& packageRoot, const std::string& relativePath,
			std::filesystem::path& resolvedPath, std::string& error);
		// 개별 이펙트 정의 파일을 읽는다.
		static bool LoadEffect(const std::filesystem::path& packageRoot, const std::filesystem::path& manifestPath,
			EffectDefinition& effect, std::string& error);

	public:
		// 패키지 정의 파일과 포함된 이펙트를 읽는다.
		static bool Load(const std::filesystem::path& manifestPath, ShaderPackage& package, std::string& error);
	};
}
