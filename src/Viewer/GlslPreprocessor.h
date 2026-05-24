#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Chrivent {
	class GlslPreprocessor {
		static constexpr std::string_view kIncludePrefix = "#include \"";

		// include 지시문에서 상대 경로를 추출한다.
		static std::optional<std::filesystem::path> ParseIncludePath(std::string_view line);
		// 현재 파일 기준으로 include 지시문을 재귀적으로 해석한다.
		static bool ResolveIncludes(
			const std::filesystem::path& file,
			std::string& outCode,
			std::string& outError,
			std::vector<std::filesystem::path>& includeStack);

	public:
		// GLSL 파일을 읽고 include 지시문을 해석한 최종 소스를 만든다.
		static bool LoadSource(
			const std::filesystem::path& file,
			std::string& outCode,
			std::string& outError);
	};
}
