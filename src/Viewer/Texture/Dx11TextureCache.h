#pragma once

#include "Viewer/Texture/TextureCache.h"
#include "Viewer/Error/GraphicsError.h"

#include <filesystem>
#include <d3d11.h>
#include <optional>
#include <wrl/client.h>

namespace Chrivent {
	// 캐시 정보와 D3D11 texture 및 shader resource view를 보관한다.
	struct Dx11Texture {
		bool hasAlpha = false;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>             texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>   textureView;
	};

	// 이미지 파일을 D3D11 texture로 변환하고 공통 키로 재사용한다.
	class Dx11TextureCache : public TextureCache<Dx11Texture> {
	public:
		// 텍스처가 없는 재질에 사용할 1x1 흰색 DX11 텍스처를 생성한다.
		GraphicsError::Result<Dx11Texture> CreateWhiteTexture(ID3D11Device* device);
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
		GraphicsError::Result<std::optional<Dx11Texture>> Load(
			ID3D11Device* device, const std::filesystem::path& texturePath);
	};
}
