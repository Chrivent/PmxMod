#pragma once

#include "../TextureCache.h"

namespace Chrivent {
	struct VulkanTexture : Texture {
	};

	class VulkanTextureCache : public TextureCache {
	public:
		// ?띿뒪泥섎? 罹먯떆?먯꽌 李얘굅???뚯씪?먯꽌 濡쒕뱶??Vulkan ?띿뒪泥섎줈 諛섑솚?쒕떎.
		VulkanTexture Load(const std::filesystem::path& texturePath);
	};
}
