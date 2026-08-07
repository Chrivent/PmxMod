#pragma once

#include <expected>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace Chrivent {
	struct EffectPassDefinition;
	struct ShaderProgramDefinition;

	// 외부 후처리 HLSL의 include 경로와 실제 GPU 바인딩이 실행 계약을 만족하는지 검증한다.
	class PostProcessShaderValidator {
		// include 순환을 막고 검증한 파일을 중복해서 읽지 않도록 상태를 보관한다.
		struct SourceValidationState {
			std::filesystem::path includeRoot;
			std::set<std::filesystem::path> validatedFiles;
		};

		// 정규화한 경로가 허용된 include 루트 내부인지 확인한다.
		static bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& path);
		// 한 HLSL 파일의 리터럴 include를 재귀적으로 검증한다.
		static std::expected<void, std::string> ValidateSourceFile(
			const std::filesystem::path& sourcePath, SourceValidationState& state);
		// 프로그램의 최상위 HLSL과 모든 include가 허용된 루트 안에 있는지 검증한다.
		static std::expected<void, std::string> ValidateIncludes(const ShaderProgramDefinition& program);
		// 컴파일한 shader stage의 리소스 바인딩을 패스 입력 선언과 비교한다.
		static std::expected<void, std::string> ValidateStage(const ShaderProgramDefinition& program,
			const std::string& entry, const wchar_t* target, const std::vector<bool>& declaredTextureSlots);

	public:
		// 후처리 패스의 HLSL 소스와 실제 컴파일 결과가 공통 입력 계약을 만족하는지 검증한다.
		static std::expected<void, std::string> Validate(const EffectPassDefinition& pass);
	};
}
