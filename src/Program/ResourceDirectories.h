#pragma once

#include <filesystem>

namespace Chrivent {
	// 실행 파일을 기준으로 프로그램이 소유한 리소스 디렉터리들을 해석한다.
	class ResourceDirectories {
		std::filesystem::path internalShaderDirectory;
		std::filesystem::path defaultToonTextureDirectory;
		std::filesystem::path shaderPackagesDirectory;

	public:
		const std::filesystem::path& GetInternalShaderDirectory() const { return internalShaderDirectory; }
		const std::filesystem::path& GetDefaultToonTextureDirectory() const { return defaultToonTextureDirectory; }
		const std::filesystem::path& GetShaderPackagesDirectory() const { return shaderPackagesDirectory; }

		// 현재 실행 파일 위치를 기준으로 모든 리소스 디렉터리를 초기화한다.
		bool Initialize();
	};
}
