#pragma once

#include "../../TextureCache.h"

#include <filesystem>
#include <glad/glad.h>

namespace Chrivent {
	struct GlfwTexture : Texture {
		GLuint texture = 0;
	};

	class GlfwTextureCache : public TextureCache {
	public:
		~GlfwTextureCache() override;

		// 텍스처가 없는 재질에 사용할 1x1 흰색 OpenGL 텍스처를 생성한다.
		GlfwTexture CreateWhiteTexture();
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
		GlfwTexture Load(const std::filesystem::path& texturePath, bool clamp = false);
	};
}
