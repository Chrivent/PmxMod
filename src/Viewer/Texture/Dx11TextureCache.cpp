#include "Viewer/Texture/Dx11TextureCache.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	GraphicsError::Result<Dx11Texture> Dx11TextureCache::CreateWhiteTexture(ID3D11Device* device) {
		if (device == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "dummy texture 생성",
				"DirectX 11 device를 사용할 수 없습니다"));
		}
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		const auto textureDescription = Dx11DescBuilder::MakeTexture2DDesc(
			1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		constexpr uint8_t white[] = { 255, 255, 255, 255 };
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = white;
		initData.SysMemPitch = 4;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		HRESULT result = device->CreateTexture2D(&textureDescription, &initData, &tex2D);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "dummy texture 생성",
				"DirectX 11 fallback texture를 만들지 못했습니다", result, true));
		}
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2DRv;
		result = device->CreateShaderResourceView(tex2D.Get(), nullptr, &tex2DRv);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "dummy texture view 생성",
				"DirectX 11 fallback texture view를 만들지 못했습니다", result, true));
		}
		Dx11Texture texture;
		texture.texture = tex2D;
		texture.textureView = tex2DRv;
		texture.hasAlpha = false;
		textures.emplace(key, texture);
		return texture;
	}

	GraphicsError::Result<std::optional<Dx11Texture>> Dx11TextureCache::Load(
		ID3D11Device* device, const std::filesystem::path& texturePath) {
		if (device == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "texture 생성",
				"DirectX 11 device를 사용할 수 없습니다"));
		}
		const TextureKey key{ TextureKind::File, texturePath };
		if (const auto texture = FindCachedTexture(key))
			return std::optional{ *texture };
		const auto [pixels, width, height, hasAlpha] = LoadImageRgba(texturePath);
		if (!pixels)
			return std::optional<Dx11Texture>{};
		const auto textureDescription = Dx11DescBuilder::MakeTexture2DDesc(
			width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = pixels.get();
		initData.SysMemPitch = 4 * width;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		HRESULT result = device->CreateTexture2D(&textureDescription, &initData, &tex2D);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "texture 생성",
				"DirectX 11 texture를 만들지 못했습니다", result, true));
		}
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2DRv;
		result = device->CreateShaderResourceView(tex2D.Get(), nullptr, &tex2DRv);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "texture view 생성",
				"DirectX 11 texture view를 만들지 못했습니다", result, true));
		}
		Dx11Texture texture;
		texture.texture = tex2D;
		texture.textureView = tex2DRv;
		texture.hasAlpha = hasAlpha;
		textures.emplace(key, texture);
		return std::optional{ texture };
	}
}
