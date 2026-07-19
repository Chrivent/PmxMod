#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace Chrivent {
	// 셰이더 컴파일에 실패한 파일과 컴파일러 진단을 함께 보관한다.
	struct ShaderCompileError {
		std::filesystem::path shaderPath;
		std::string message;

		// 컴파일된 셰이더 값 또는 현재 구조화된 오류를 반환하는 형식이다.
		template <typename T>
		using Result = std::expected<T, ShaderCompileError>;
	};
}
