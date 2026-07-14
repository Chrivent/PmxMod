#include "Viewer/Texture/GlfwTextureCache.h"

#include "Viewer/Viewer/Viewer.h"

#include <ranges>
#include <stb_image.h>

namespace Chrivent {
	GlfwTextureCache::~GlfwTextureCache() {
		for (const auto& texture : textures | std::views::values) {
			const auto glfwTexture = std::static_pointer_cast<GlfwTexture>(texture);
			const GLuint textureId = glfwTexture->texture;
			glDeleteTextures(1, &textureId);
		}
	}

	GlfwTexture GlfwTextureCache::CreateWhiteTexture() {
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture<GlfwTexture>(key))
			return *texture;
		constexpr unsigned char pixels[] = { 255, 255, 255, 255 };
		GLuint tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		glTextureStorage2D(tex, 1, GL_RGBA8, 1, 1);
		glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		const auto texture = std::make_shared<GlfwTexture>();
		texture->key = key;
		texture->texture = tex;
		texture->hasAlpha = false;
		textures[key] = texture;
		return *texture;
	}

	GlfwTexture GlfwTextureCache::Load(const std::filesystem::path& texturePath, const bool clamp) {
		const TextureKey key{ TextureKind::File, texturePath, clamp };
		if (const auto texture = FindCachedTexture<GlfwTexture>(key))
			return *texture;
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = Viewer::LoadImageRgba(texturePath, x, y, comp);
		if (!image)
			return {};
		const bool hasAlpha = comp == 4;
		GLuint tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex);
		glTextureStorage2D(tex, 1, GL_RGBA8, x, y);
		glTextureSubImage2D(tex, 0, 0, 0, x, y, GL_RGBA, GL_UNSIGNED_BYTE, image);
		stbi_image_free(image);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (clamp) {
			glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		const auto texture = std::make_shared<GlfwTexture>();
		texture->key = key;
		texture->texture = tex;
		texture->hasAlpha = hasAlpha;
		textures[key] = texture;
		return *texture;
	}
}
