#pragma once

#include "../TextureCache.h"

#include <filesystem>

namespace Chrivent {
	struct Dx12Texture : Texture {};

	class Dx12TextureCache : public TextureCache {
	public:
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX12 리소스로 반환한다.
		static Dx12Texture Load(const std::filesystem::path& texturePath);
	};
}
