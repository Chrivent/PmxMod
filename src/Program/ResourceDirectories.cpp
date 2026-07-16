#include "Program/ResourceDirectories.h"

#include <Windows.h>
#include <vector>

namespace Chrivent {
	bool ResourceDirectories::Initialize() {
		std::vector<wchar_t> buffer(MAX_PATH);
		std::filesystem::path executablePath;
		while (true) {
			const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (length == 0)
				return false;
			if (length < buffer.size()) {
				executablePath = std::filesystem::path(std::wstring(buffer.data(), length));
				break;
			}
			buffer.resize(buffer.size() * 2);
		}
		const std::filesystem::path resourceDirectory = executablePath.parent_path() / "resource";
		internalShaderDirectory = resourceDirectory / "internal" / "shaders";
		defaultToonTextureDirectory = resourceDirectory / "internal" / "toon";
		shaderPackagesDirectory = resourceDirectory / "shaders";
		return true;
	}
}
