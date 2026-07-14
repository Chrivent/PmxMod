#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <nlohmann/json_fwd.hpp>

namespace Chrivent {
	enum class EffectType {
		Model,
		Edge,
		GroundShadow,
		PostProcess
	};

	enum class EffectPassResolution {
		Full,
		Half,
		Quarter,
		Fixed
	};

	enum class EffectTextureFormat {
		Rgba8Unorm,
		Rgba16Float,
		Rgba32Float
	};

	enum class EffectResourceLifetime {
		Transient,
		History
	};

	struct EffectResourceDefinition {
		std::string name;
		EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
		EffectPassResolution resolution = EffectPassResolution::Full;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct EffectPassInputDefinition {
		uint32_t slot = 0;
		std::string resource;
	};

	struct EffectPassDefinition {
		std::string name;
		std::filesystem::path shaderPath;
		std::string vertexEntry = "VSMain";
		std::string pixelEntry = "PSMain";
		std::vector<EffectPassInputDefinition> inputs;
		std::string output;
	};

	struct EffectDefinition {
		std::string id;
		std::string name;
		EffectType type = EffectType::PostProcess;
		std::vector<std::string> inputs;
		std::vector<EffectResourceDefinition> resources;
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
		static constexpr int schemaVersion = 1;

		// JSON 파일을 읽고 최상위 객체인지 확인한다.
		static bool ReadJsonObject(const std::filesystem::path& path, nlohmann::json& json, std::string& error);
		// 필수 문자열 필드를 읽는다.
		static bool ReadRequiredString(const nlohmann::json& json, const char* key, std::string& value, std::string& error);
		// 경로가 지정한 루트 내부인지 확인한다.
		static bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& path);
		// 패키지 내부의 상대 경로를 실제 파일 경로로 변환한다.
		static bool ResolvePackagePath(const std::filesystem::path& packageRoot, const std::string& relativePath,
			std::filesystem::path& resolvedPath, std::string& error);
		// JSON 객체 하나를 셰이더 pass 정의로 변환한다.
		static bool LoadPass(const std::filesystem::path& packageRoot, const std::filesystem::path& manifestPath,
			const nlohmann::json& json, bool postProcess, EffectPassDefinition& pass, std::string& error);
		// 후처리 effect가 소유하는 범용 texture 리소스 정의를 읽는다.
		static bool LoadResources(const nlohmann::json& json, const std::filesystem::path& manifestPath,
			std::vector<EffectResourceDefinition>& resources, std::string& error);
		// 개별 이펙트 정의 파일을 읽는다.
		static bool LoadEffect(const std::filesystem::path& packageRoot, const std::filesystem::path& manifestPath,
			EffectDefinition& effect, std::string& error);

	public:
		// 패키지 정의 파일과 포함된 이펙트를 읽는다.
		static bool Load(const std::filesystem::path& manifestPath, ShaderPackage& package, std::string& error);
	};

	struct BuiltInShaderPasses {
		EffectPassDefinition model;
		EffectPassDefinition edge;
		EffectPassDefinition groundShadow;
	};

	class BuiltInShaderContract {
	public:
		// 내장 패키지를 읽고 모델, 엣지, 지면 그림자 단일 패스 계약을 검증한다.
		static bool Load(const std::filesystem::path& manifestPath, BuiltInShaderPasses& passes, std::string& error);

	private:
		// 지정한 역할의 이펙트가 하나의 패스로 유일하게 선언됐는지 확인한다.
		static bool ResolvePass(const ShaderPackage& package, EffectType type, const char* role,
			EffectPassDefinition& pass, std::string& error);
	};
}
