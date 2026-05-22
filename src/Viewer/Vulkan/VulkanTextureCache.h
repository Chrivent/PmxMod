#pragma once

#include "../TextureCache.h"

namespace Chrivent {
	struct VulkanTexture : Texture {
	};

	class VulkanTextureCache : public TextureCache {
	public:
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		VulkanTexture Load(const std::filesystem::path& texturePath);
	};
}
