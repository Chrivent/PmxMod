#include "Viewer/Texture/OpenGlTextureCache.h"

#include "Viewer/Error/OpenGlErrorState.h"

#include <ranges>

namespace Chrivent {
	OpenGlTextureCache::~OpenGlTextureCache() {
		for (const auto& texture : textures | std::views::values) {
			const GLuint textureId = texture.texture;
			glDeleteTextures(1, &textureId);
		}
	}

	GraphicsError::Result<OpenGlTexture> OpenGlTextureCache::CreateWhiteTexture() {
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		constexpr unsigned char pixels[] = { 255, 255, 255, 255 };
		GLuint tex = 0;
		OpenGlErrorState::Clear();
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		if (tex == 0) {
			const GLenum result = OpenGlErrorState::Take();
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "dummy texture 생성",
				"OpenGL texture 객체를 만들지 못했습니다",
				result, result != GL_NO_ERROR));
		}
		glTextureStorage2D(tex, 1, GL_RGBA8, 1, 1);
		glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		const GLenum result = OpenGlErrorState::Take();
		if (result != GL_NO_ERROR) {
			glDeleteTextures(1, &tex);
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "dummy texture 생성",
				"OpenGL fallback texture를 초기화하지 못했습니다", result, true));
		}
		const OpenGlTexture texture{ .hasAlpha = false, .texture = tex };
		textures.emplace(key, texture);
		return texture;
	}

	GraphicsError::Result<std::optional<OpenGlTexture>> OpenGlTextureCache::Load(
		const std::filesystem::path& texturePath, const bool clamp) {
		const TextureKey key{ TextureKind::File, texturePath, clamp };
		if (const auto texture = FindCachedTexture(key))
			return std::optional{ *texture };
		const auto [pixels, width, height, hasAlpha] = LoadImageRgba(texturePath);
		if (!pixels)
			return std::optional<OpenGlTexture>{};
		GLuint tex = 0;
		OpenGlErrorState::Clear();
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		if (tex == 0) {
			const GLenum result = OpenGlErrorState::Take();
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "texture 생성",
				"OpenGL texture 객체를 만들지 못했습니다",
				result, result != GL_NO_ERROR));
		}
		glTextureStorage2D(tex, 1, GL_RGBA8, width, height);
		glTextureSubImage2D(tex, 0, 0, 0, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (clamp) {
			glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		const GLenum result = OpenGlErrorState::Take();
		if (result != GL_NO_ERROR) {
			glDeleteTextures(1, &tex);
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "texture 생성",
				"OpenGL texture 데이터를 업로드하지 못했습니다", result, true));
		}
		const OpenGlTexture texture{ .hasAlpha = hasAlpha, .texture = tex };
		textures.emplace(key, texture);
		return std::optional{ texture };
	}
}
