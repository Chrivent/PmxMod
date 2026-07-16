#include "Viewer/Texture/OpenGlTextureCache.h"

#include <ranges>

namespace Chrivent {
	OpenGlTextureCache::~OpenGlTextureCache() {
		for (const auto& texture : textures | std::views::values) {
			const GLuint textureId = texture.texture;
			glDeleteTextures(1, &textureId);
		}
	}

	OpenGlTexture OpenGlTextureCache::CreateWhiteTexture() {
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		constexpr unsigned char pixels[] = { 255, 255, 255, 255 };
		GLuint tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		glTextureStorage2D(tex, 1, GL_RGBA8, 1, 1);
		glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		const OpenGlTexture texture{ .hasAlpha = false, .texture = tex };
		textures.emplace(key, texture);
		return texture;
	}

	OpenGlTexture OpenGlTextureCache::Load(const std::filesystem::path& texturePath, const bool clamp) {
		const TextureKey key{ TextureKind::File, texturePath, clamp };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		const auto [pixels, width, height, components] = LoadImageRgba(texturePath);
		if (!pixels)
			return {};
		const bool hasAlpha = components == 4;
		GLuint tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		glTextureStorage2D(tex, 1, GL_RGBA8, width, height);
		glTextureSubImage2D(tex, 0, 0, 0, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (clamp) {
			glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		const OpenGlTexture texture{ .hasAlpha = hasAlpha, .texture = tex };
		textures.emplace(key, texture);
		return texture;
	}
}
