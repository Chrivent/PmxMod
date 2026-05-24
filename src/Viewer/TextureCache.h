#pragma once

#include <filesystem>
#include <map>
#include <memory>

namespace Chrivent {
	enum class TextureKind {
		File,
		White
	};

	struct TextureKey {
		TextureKind kind = TextureKind::File;
		std::filesystem::path path;
		bool clamp = false;

		bool operator<(const TextureKey& other) const {
			if (kind != other.kind)
				return kind < other.kind;
			if (path != other.path)
				return path < other.path;
			return clamp < other.clamp;
		}
	};

	struct Texture {
		TextureKey key;
		bool hasAlpha = false;

		virtual ~Texture() = default;
	};

	class TextureCache {
	protected:
		std::map<TextureKey, std::shared_ptr<Texture>> textures;

	public:
		virtual ~TextureCache() = default;
	};
}
