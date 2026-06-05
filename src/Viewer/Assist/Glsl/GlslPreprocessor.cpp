#include "GlslPreprocessor.h"

#include <fstream>
#include <sstream>

namespace Chrivent {
	bool GlslPreprocessor::LoadSource(
		const std::filesystem::path& file,
		const std::string& preamble,
		std::string& outCode,
		std::string& outError) {
		const auto normalizedPath = std::filesystem::weakly_canonical(file);
		const std::ifstream stream(normalizedPath);
		if (!stream) {
			outError = "Failed to open GLSL shader file: " + normalizedPath.string();
			return false;
		}
		std::ostringstream builder;
		builder << stream.rdbuf();
		const std::string source = builder.str();
		outCode.clear();
		outCode.reserve(preamble.size() + source.size() + 1);
		outCode.append(preamble);
		if (!preamble.empty() && preamble.back() != '\n')
			outCode.push_back('\n');
		outCode.append(source);
		outError.clear();
		return true;
	}
}
