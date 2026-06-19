#pragma once

#include <filesystem>
#include <vector>

namespace Chrivent {
	struct ModelConfig {
		std::filesystem::path modelPath;
		std::vector<std::filesystem::path> animPaths;
		float scale = 1.0f;
	};

	struct SceneConfig {
		std::vector<ModelConfig> modelConfigs;
		std::filesystem::path cameraAnim;
		std::filesystem::path musicPath;

		// 씬 설정 파일을 읽어 모델, 카메라, 음악 경로를 구성한다.
		bool Load(const std::filesystem::path& filepath);
		// 현재 씬 설정을 파일로 저장한다.
		bool Save(const std::filesystem::path& filepath) const;
	};
}
