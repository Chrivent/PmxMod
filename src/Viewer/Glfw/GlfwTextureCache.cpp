#include "GlfwTextureCache.h"

#include "../Viewer.h"

#include <ranges>
#include <stb_image.h>

namespace Chrivent {
	GlfwTextureCache::~GlfwTextureCache() {
		for (const auto& texture : textures | std::views::values) {
			const GLuint textureId = texture.texture;
			glDeleteTextures(1, &textureId);
		}
	}

	GlfwTexture GlfwTextureCache::Load(const std::filesystem::path& texturePath, const bool clamp) {
		const auto it = textures.find(texturePath);
		if (it != textures.end())
			return it->second;
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = Viewer::LoadImageRgba(texturePath, x, y, comp, true);
		if (!image)
			return {};
		const bool hasAlpha = comp == 4;
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0,
			GL_RGBA, x, y, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, image);
		stbi_image_free(image);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (clamp) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		GlfwTexture texture;
		texture.texture = tex;
		texture.hasAlpha = hasAlpha;
		textures[texturePath] = texture;
		return textures[texturePath];
	}
}
