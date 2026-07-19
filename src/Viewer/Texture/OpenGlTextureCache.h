#pragma once

#include "Viewer/Texture/TextureCache.h"
#include "Viewer/Error/GraphicsError.h"

#include <filesystem>
#include <glad/glad.h>
#include <optional>

namespace Chrivent {
	// 캐시 정보와 OpenGL texture 객체를 보관한다.
	struct OpenGlTexture {
		bool hasAlpha = false;
		GLuint texture = 0;
	};

	// 이미지 파일을 OpenGL texture로 변환하고 공통 키로 재사용한다.
	class OpenGlTextureCache : public TextureCache<OpenGlTexture> {
	public:
		OpenGlTextureCache() = default;
		OpenGlTextureCache(const OpenGlTextureCache&) = delete;
		OpenGlTextureCache& operator=(const OpenGlTextureCache&) = delete;

		~OpenGlTextureCache();

		// 텍스처가 없는 재질에 사용할 1x1 흰색 OpenGL 텍스처를 생성한다.
		GraphicsResult<OpenGlTexture> CreateWhiteTexture();
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
		GraphicsResult<std::optional<OpenGlTexture>> Load(
			const std::filesystem::path& texturePath, bool clamp = false);
	};
}
