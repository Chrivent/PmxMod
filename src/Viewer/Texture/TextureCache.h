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

		// 캐시에 저장된 렌더러별 텍스처를 찾는다.
		template <typename TextureType>
		std::shared_ptr<TextureType> FindCachedTexture(const TextureKey& key) const {
			const auto it = textures.find(key);
			if (it == textures.end())
				return nullptr;
			return std::static_pointer_cast<TextureType>(it->second);
		}

	public:
		virtual ~TextureCache() = default;
	};
}
