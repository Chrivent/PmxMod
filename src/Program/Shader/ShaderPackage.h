#pragma once

#include "Viewer/PostProcess/PostProcessRuntimeContract.h"

#include <expected>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace Chrivent {
	// 셰이더 패키지 검색과 검증 실패의 성격을 구분한다.
	enum class ShaderPackageErrorCode {
		ScanFailed,
		InvalidPackage,
		DuplicatePackageId
	};

	// 셰이더 패키지 실패 원인과 관련 manifest 경로를 보관한다.
	struct ShaderPackageError {
		ShaderPackageErrorCode code = ShaderPackageErrorCode::InvalidPackage;
		std::filesystem::path manifestPath;
		std::string message;

		// 프로그램 경계에서 한 번 출력할 진단 문자열을 생성한다.
		std::string Format() const;
	};

	// UI와 모션 키에 노출할 효과 파라미터의 이름, 범위와 기본값을 보관한다.
	struct EffectParameterMetadata {
		std::string id;
		std::string name;
		uint32_t slot = 0;
		float defaultValue = 0.0f;
		float minimumValue = 0.0f;
		float maximumValue = 1.0f;
	};

	// 프로그램에 노출할 효과 메타데이터와 렌더러 실행 계약을 묶는다.
	struct EffectDefinition {
		std::string id;
		std::string name;
		std::vector<EffectParameterMetadata> parameters;
		EffectRuntimeDefinition runtime;
	};

	// 렌더러가 소비하는 이펙트와 패스 정의를 하나의 설치 단위로 묶는다.
	struct ShaderPackage {
		std::string id;
		std::string name;
		std::string version;
		std::string author;
		std::vector<EffectDefinition> effects;
	};

	// 패키지 검색 결과와 패키지별 진단 메시지를 함께 반환한다.
	struct ShaderPackageDiscovery {
		std::vector<ShaderPackage> packages;
		std::vector<ShaderPackageError> errors;
	};

	// 셰이더 패키지 디렉터리를 탐색하고 유효한 패키지를 수집한다.
	class ShaderPackageLoader {
	public:
		// 지정한 디렉터리 바로 아래에서 셰이더 패키지를 검색한다.
		static ShaderPackageDiscovery Discover(const std::filesystem::path& packagesDirectory);
	};

	// 패키지 manifest와 효과 JSON을 검증해 실행 가능한 선언으로 변환한다.
	class ShaderPackageParser {
		// JSON에서 읽은 효과 리소스 이름과 실행 속성을 함께 보관한다.
		struct ParsedEffectResource {
			std::string name;
			EffectResourceDefinition definition;
		};

		// 정규화된 리소스 색인과 수명을 파서 내부 이름 조회 결과로 보관한다.
		struct ParsedResourceReference {
			size_t index = 0;
			EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		};

		// JSON에서 읽은 후처리 패스 texture 입력을 보관한다.
		struct ParsedEffectPassInput {
			uint32_t slot = 0;
			std::string resource;
		};

		// JSON에서 읽은 후처리 패스 메타데이터와 문자열 입출력을 보관한다.
		struct ParsedEffectPass {
			std::string name;
			ShaderProgramDefinition program;
			std::vector<ParsedEffectPassInput> inputs;
			std::string output;
		};

		static constexpr int schemaVersion = 1;

		// JSON 파일을 읽고 최상위 객체인지 확인한다.
		static bool ReadJsonObject(const std::filesystem::path& path, nlohmann::json& json, std::string& error);
		// 필수 문자열 필드를 읽는다.
		static bool ReadRequiredString(const nlohmann::json& json, const char* key,
			std::string& value, std::string& error);
		// 경로가 지정한 루트 내부인지 확인한다.
		static bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& path);
		// 패키지 내부의 상대 경로를 실제 파일 경로로 변환한다.
		static bool ResolvePackagePath(const std::filesystem::path& packageRoot,
			const std::string& relativePath, std::filesystem::path& resolvedPath, std::string& error);
		// JSON 객체 하나를 셰이더 pass 정의로 변환한다.
		static bool LoadPass(const std::filesystem::path& packageRoot,
			const std::filesystem::path& manifestPath, const nlohmann::json& json,
			ParsedEffectPass& pass, std::string& error);
		// 후처리 effect가 소유하는 범용 texture 리소스 정의를 읽는다.
		static bool LoadResources(const nlohmann::json& json, const std::filesystem::path& manifestPath,
			std::vector<ParsedEffectResource>& resources, std::string& error);
		// 후처리 effect가 b1에서 사용할 스칼라 파라미터 선언을 읽는다.
		static bool LoadParameters(const nlohmann::json& json, const std::filesystem::path& manifestPath,
			std::vector<EffectParameterMetadata>& metadata,
			std::vector<EffectParameterValue>& values, std::string& error);
		// 개별 이펙트 정의 파일을 읽는다.
		static bool LoadEffect(const std::filesystem::path& packageRoot,
			const std::filesystem::path& manifestPath, EffectDefinition& effect, std::string& error);
		// 패키지 manifest를 기존 파서 단계에 전달하고 출력 객체를 완성한다.
		static bool LoadPackage(const std::filesystem::path& manifestPath,
			ShaderPackage& package, std::string& error);

	public:
		// 패키지 정의 파일과 포함된 이펙트를 읽는다.
		static std::expected<ShaderPackage, ShaderPackageError> Load(const std::filesystem::path& manifestPath);
	};
}
