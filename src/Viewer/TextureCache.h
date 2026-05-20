#pragma once

#include <filesystem>
#include <map>
#include <memory>

namespace Chrivent {
	struct Texture {
		bool hasAlpha = false;

		virtual ~Texture() = default;
	};

	class TextureCache {
	protected:
		std::map<std::filesystem::path, std::shared_ptr<Texture>> textures;

	public:
		virtual ~TextureCache() = default;
	};
}
