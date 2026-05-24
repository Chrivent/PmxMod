#include "GlslPreprocessor.h"

#include <fstream>
#include <sstream>

namespace Chrivent {
	std::optional<std::filesystem::path> GlslPreprocessor::ParseIncludePath(const std::string_view line) {
		if (!line.starts_with(kIncludePrefix))
			return std::nullopt;
		constexpr size_t pathStart = kIncludePrefix.size();
		const size_t pathEnd = line.find('"', pathStart);
		if (pathEnd == std::string_view::npos)
			return std::nullopt;
		return std::filesystem::path(line.substr(pathStart, pathEnd - pathStart));
	}

	bool GlslPreprocessor::ResolveIncludes(
		const std::filesystem::path& file,
		std::string& outCode,
		std::string& outError,
		std::vector<std::filesystem::path>& includeStack) {
		const auto normalizedPath = std::filesystem::weakly_canonical(file);
		for (const auto& includePath : includeStack) {
			if (includePath == normalizedPath) {
				outError = "Failed to resolve GLSL include cycle: " + normalizedPath.string();
				return false;
			}
		}
		std::ifstream stream(normalizedPath);
		if (!stream) {
			outError = "Failed to open GLSL shader file: " + normalizedPath.string();
			return false;
		}
		includeStack.push_back(normalizedPath);
		std::ostringstream builder;
		std::string line;
		while (std::getline(stream, line)) {
			const auto includePath = ParseIncludePath(line);
			if (includePath.has_value()) {
				std::string includedCode;
				if (!ResolveIncludes(normalizedPath.parent_path() / *includePath, includedCode, outError, includeStack)) {
					includeStack.pop_back();
					return false;
				}
				builder << includedCode;
				continue;
			}
			if (line.starts_with(kIncludePrefix)) {
				outError = "Invalid GLSL include directive: " + normalizedPath.string();
				includeStack.pop_back();
				return false;
			}
			builder << line << '\n';
		}
		includeStack.pop_back();
		outCode = builder.str();
		return true;
	}

	bool GlslPreprocessor::LoadSource(
		const std::filesystem::path& file,
		std::string& outCode,
		std::string& outError) {
		std::vector<std::filesystem::path> includeStack;
		return ResolveIncludes(file, outCode, outError, includeStack);
	}
}
