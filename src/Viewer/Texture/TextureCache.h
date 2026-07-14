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
		struct ImageDeleter {
			// stb_image가 할당한 픽셀 메모리를 해제한다.
			void operator()(unsigned char* pixels) const;
		};

		struct LoadedImage {
			std::unique_ptr<unsigned char, ImageDeleter> pixels;
			int width = 0;
			int height = 0;
			int components = 0;
		};

		std::map<TextureKey, std::shared_ptr<Texture>> textures;

		// 이미지 파일을 자동 해제되는 RGBA 픽셀 데이터로 읽는다.
		static LoadedImage LoadImageRgba(const std::filesystem::path& texturePath);

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
