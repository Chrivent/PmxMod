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
		Eighth,
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

	// 효과 패키지가 생성할 중간 또는 history texture의 선언을 나타낸다.
	struct EffectResourceDefinition {
		std::string name;
		EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
		EffectPassResolution resolution = EffectPassResolution::Full;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// 효과 패스의 texture 슬롯과 참조할 리소스 이름을 연결한다.
	struct EffectPassInputDefinition {
		uint32_t slot = 0;
		std::string resource;
	};

	// 셰이더 파일과 진입점 및 입출력으로 구성된 렌더링 패스를 나타낸다.
	struct EffectPassDefinition {
		std::string name;
		std::filesystem::path shaderPath;
		std::string vertexEntry = "VSMain";
		std::string pixelEntry = "PSMain";
		std::vector<EffectPassInputDefinition> inputs;
		std::string output;
	};

	// 효과가 b1 상수 버퍼에서 사용할 스칼라 파라미터 한 개를 선언한다.
	struct EffectParameterDefinition {
		std::string id;
		std::string name;
		uint32_t slot = 0;
		float defaultValue = 0.0f;
		float minimumValue = 0.0f;
		float maximumValue = 1.0f;
	};

	// 사용자에게 노출할 효과 하나의 타입과 리소스 및 패스 목록을 나타낸다.
	struct EffectDefinition {
		std::string id;
		std::string name;
		EffectType type = EffectType::PostProcess;
		std::vector<std::string> inputs;
		std::vector<EffectParameterDefinition> parameters;
		std::vector<EffectResourceDefinition> resources;
		std::vector<EffectPassDefinition> passes;
	};

	// 렌더러가 소비하는 이펙트와 패스 정의를 하나의 설치 단위로 묶는다.
	struct ShaderPackage {
		std::string id;
		std::string name;
		std::string version;
		std::string author;
		std::filesystem::path rootPath;
		std::vector<EffectDefinition> effects;
	};

	// 패키지 검색 결과와 패키지별 진단 메시지를 함께 반환한다.
	struct ShaderPackageDiscovery {
		std::vector<ShaderPackage> packages;
		std::vector<std::string> errors;
	};

	// 셰이더 패키지 디렉터리를 탐색하고 유효한 패키지를 수집한다.
	class ShaderPackageLoader {
	public:
		// 지정한 디렉터리 바로 아래에서 셰이더 패키지를 검색한다.
		static ShaderPackageDiscovery Discover(const std::filesystem::path& packagesDirectory);
	};
	
	// 패키지 manifest와 효과 JSON을 검증해 실행 가능한 선언으로 변환한다.
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
		// 후처리 effect가 b1에서 사용할 스칼라 파라미터 선언을 읽는다.
		static bool LoadParameters(const nlohmann::json& json, const std::filesystem::path& manifestPath,
			std::vector<EffectParameterDefinition>& parameters, std::string& error);
		// 개별 이펙트 정의 파일을 읽는다.
		static bool LoadEffect(const std::filesystem::path& packageRoot, const std::filesystem::path& manifestPath,
			EffectDefinition& effect, std::string& error);

	public:
		// 패키지 정의 파일과 포함된 이펙트를 읽는다.
		static bool Load(const std::filesystem::path& manifestPath, ShaderPackage& package, std::string& error);
	};

	// 내장 모델 렌더링이 요구하는 표면·외곽선·지면 그림자 패스를 보관한다.
	struct BuiltInShaderPasses {
		EffectPassDefinition model;
		EffectPassDefinition edge;
		EffectPassDefinition groundShadow;
	};

	// API 구현에서 파일명과 진입점을 알지 않도록 엔진 장면 입력 패스를 역할별로 보관한다.
	struct SceneInputShaderPasses {
		EffectPassDefinition depth;
		EffectPassDefinition velocity;
		EffectPassDefinition velocityInvertedY;
	};

	// 내장 셰이더 패키지가 필수 렌더링 역할 계약을 충족하는지 검증한다.
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
