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

		bool Load(const std::filesystem::path& filepath);
		bool Save(const std::filesystem::path& filepath) const;
	};
}
