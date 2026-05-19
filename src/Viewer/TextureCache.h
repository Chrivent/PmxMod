#pragma once

#include <filesystem>
#include <map>

namespace Chrivent {
	struct Texture {
		bool hasAlpha = false;
	};

	template <typename TTexture>
	class TextureCache {
	protected:
		std::map<std::filesystem::path, TTexture> textures;

	public:
		virtual ~TextureCache() = default;
	};
}