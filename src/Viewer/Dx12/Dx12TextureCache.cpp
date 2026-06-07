#include "Dx12TextureCache.h"

namespace Chrivent {
	Dx12Texture Dx12TextureCache::Load(const std::filesystem::path& texturePath) {
		Dx12Texture texture;
		texture.key = TextureKey{ TextureKind::File, texturePath };
		return texture;
	}
}
