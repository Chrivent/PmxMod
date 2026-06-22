#pragma once

#include "../../TextureCache.h"
#include "Helper/Dx12Device.h"

#include <filesystem>
#include <wrl/client.h>

namespace Chrivent {
	struct Dx12Texture : Texture {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		UINT width = 0;
		UINT height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	};

	class Dx12TextureCache : public TextureCache {
		// RGBA 픽셀을 DX12 texture resource로 업로드한다.
		static bool UploadRgbaPixels(const Dx12Device& sourceDevice, const unsigned char* pixels,
			UINT width, UINT height, Dx12Texture& texture);

	public:
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX12 리소스로 반환한다.
		Dx12Texture Load(const Dx12Device& sourceDevice, const std::filesystem::path& texturePath);
		// 텍스처가 없는 material에 바인딩할 흰색 DX12 텍스처를 생성한다.
		Dx12Texture CreateWhiteTexture(const Dx12Device& sourceDevice);
	};
}
