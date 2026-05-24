#include "Dx11TextureCache.h"

#include "Helper/Dx11DescriptorFactory.h"
#include "../Viewer.h"

#include <stb_image.h>

namespace Chrivent {
	Dx11Texture Dx11TextureCache::CreateWhiteTexture(ID3D11Device* device) {
		const std::filesystem::path key("__dummy_white__");
		const auto it = textures.find(key);
		if (it != textures.end()) {
			const auto texture = std::dynamic_pointer_cast<Dx11Texture>(it->second);
			return texture ? *texture : Dx11Texture{};
		}
		const auto d = Dx11DescriptorFactory::MakeTexture2DDesc(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
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
		texture->texture = tex2D;
		texture->textureView = tex2DRv;
		texture->hasAlpha = false;
		textures[key] = texture;
		return *texture;
	}

	Dx11Texture Dx11TextureCache::Load(ID3D11Device* device, const std::filesystem::path& texturePath) {
		const auto it = textures.find(texturePath);
		if (it != textures.end()) {
			const auto texture = std::dynamic_pointer_cast<Dx11Texture>(it->second);
			return texture ? *texture : Dx11Texture{};
		}
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = Viewer::LoadImageRgba(texturePath, x, y, comp);
		if (!image)
			return {};
		const bool textureHasAlpha = comp == 4;
		const auto d = Dx11DescriptorFactory::MakeTexture2DDesc(
			x, y, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = image;
		initData.SysMemPitch = 4 * x;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		const HRESULT hr = device->CreateTexture2D(&d, &initData, &tex2D);
		stbi_image_free(image);
		if (FAILED(hr))
			return {};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2DRv;
		if (FAILED(device->CreateShaderResourceView(tex2D.Get(), nullptr, &tex2DRv)))
			return {};
		const auto tex = std::make_shared<Dx11Texture>();
		tex->texture = tex2D;
		tex->textureView = tex2DRv;
		tex->hasAlpha = textureHasAlpha;
		textures[texturePath] = tex;
		return *tex;
	}
}
