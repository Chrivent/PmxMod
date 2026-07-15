#pragma once

#include <filesystem>
#include <map>
#include <memory>

namespace Chrivent {
	enum class TextureKind {
		File,
		White
	};

	// texture 종류와 파일 경로 및 sampler 방식으로 캐시 항목을 식별한다.
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

	// 렌더링 API별 texture가 공유하는 캐시 키와 alpha 정보를 보관한다.
	struct Texture {
		TextureKey key;
		bool hasAlpha = false;
	};

	// 이미지 디코딩과 렌더링 API별 texture 캐시 저장소를 제공한다.
	class TextureCache {
	protected:
		// stb_image가 할당한 픽셀 메모리를 unique_ptr에서 해제한다.
		struct ImageDeleter {
			// stb_image가 할당한 픽셀 메모리를 해제한다.
			void operator()(unsigned char* pixels) const;
		};

		// 디코딩된 RGBA 픽셀과 원본 이미지 정보를 함께 보관한다.
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
	};
}
