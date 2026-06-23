#pragma once

#include "Viewer/TextureCache.h"

#include <filesystem>
#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	struct Dx11Texture : Texture {
		Microsoft::WRL::ComPtr<ID3D11Texture2D>             texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>   textureView;
	};

	class Dx11TextureCache : public TextureCache {
	public:
		// 텍스처가 없는 재질에 사용할 1x1 흰색 DX11 텍스처를 생성한다.
		Dx11Texture CreateWhiteTexture(ID3D11Device* device);
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
		Dx11Texture Load(ID3D11Device* device, const std::filesystem::path& texturePath);
	};
}
