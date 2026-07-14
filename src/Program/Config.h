#pragma once

#include <filesystem>
#include <vector>

namespace Chrivent {
	// 씬에 배치할 PMX 모델과 모션 파일 경로를 보관한다.
	struct ModelConfig {
		std::filesystem::path modelPath;
		std::vector<std::filesystem::path> animPaths;
		float scale = 1.0f;
	};

	// 모델, 카메라와 음악으로 구성된 씬 설정을 보관한다.
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
