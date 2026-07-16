#include "Viewer/Texture/Dx11TextureCache.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	Dx11Texture Dx11TextureCache::CreateWhiteTexture(ID3D11Device* device) {
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		const auto d = Dx11DescBuilder::MakeTexture2DDesc(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		constexpr uint8_t white[] = { 255, 255, 255, 255 };
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = white;
		initData.SysMemPitch = 4;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		if (FAILED(device->CreateTexture2D(&d, &initData, &tex2D)))
			return {};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2DRv;
		if (FAILED(device->CreateShaderResourceView(tex2D.Get(), nullptr, &tex2DRv)))
			return {};
		const auto texture = std::make_shared<Dx11Texture>();
		texture->key = key;
		texture->texture = tex2D;
		texture->textureView = tex2DRv;
		texture->hasAlpha = false;
		textures[key] = texture;
		return *texture;
	}

	Dx11Texture Dx11TextureCache::Load(ID3D11Device* device, const std::filesystem::path& texturePath) {
		const TextureKey key{ TextureKind::File, texturePath };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		const auto [pixels, width, height, components] = LoadImageRgba(texturePath);
		if (!pixels)
			return {};
		const bool textureHasAlpha = components == 4;
		const auto d = Dx11DescBuilder::MakeTexture2DDesc(
			width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = pixels.get();
		initData.SysMemPitch = 4 * width;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		const HRESULT hr = device->CreateTexture2D(&d, &initData, &tex2D);
		if (FAILED(hr))
			return {};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2DRv;
		if (FAILED(device->CreateShaderResourceView(tex2D.Get(), nullptr, &tex2DRv)))
			return {};
		const auto tex = std::make_shared<Dx11Texture>();
		tex->key = key;
		tex->texture = tex2D;
		tex->textureView = tex2DRv;
		tex->hasAlpha = textureHasAlpha;
		textures[key] = tex;
		return *tex;
	}
}
