#pragma once

#include <filesystem>
#include <string>

namespace Chrivent {
	class GlslPreprocessor {
	public:
		// GLSL 파일 앞에 백엔드별 preamble을 붙여 최종 소스를 만든다.
		static bool LoadSource(const std::filesystem::path& file, const std::string& preamble, std::string& outCode, std::string& outError);
	};
}
