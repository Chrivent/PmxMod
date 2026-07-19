#include "Viewer/Texture/Dx12TextureCache.h"

namespace Chrivent {
	GraphicsResult<void> Dx12TextureCache::UploadRgbaPixels(
		const Dx12Device& sourceDevice, const unsigned char* pixels,
		const UINT width, const UINT height, Dx12Texture& texture) const {
		if (!sourceDevice.GetDevice() || !sourceDevice.GetCommandQueue()
			|| pixels == nullptr || width == 0 || height == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "texture 업로드",
				"DirectX 12 device, 픽셀 데이터 또는 texture 크기가 올바르지 않습니다"));
		}
		D3D12_RESOURCE_DESC textureDesc{};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		defaultHeap.CreationNodeMask = 1;
		defaultHeap.VisibleNodeMask = 1;
		HRESULT result = sourceDevice.GetDevice()->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture.resource));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "texture 생성",
				"DirectX 12 texture 리소스를 만들지 못했습니다", result, true));
		}
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
		UINT rowCount = 0;
		UINT64 uploadByteSize = 0;
		sourceDevice.GetDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0,
			&layout, &rowCount, nullptr, &uploadByteSize);
		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeap.CreationNodeMask = 1;
		uploadHeap.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC uploadDesc{};
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width = uploadByteSize;
		uploadDesc.Height = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.SampleDesc.Count = 1;
		uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		result = sourceDevice.GetDevice()->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "texture upload buffer 생성",
				"DirectX 12 texture upload buffer를 만들지 못했습니다", result, true));
		}
		void* mappedData = nullptr;
		constexpr D3D12_RANGE readRange{ 0, 0 };
		result = uploadBuffer->Map(0, &readRange, &mappedData);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "texture upload buffer 매핑",
				"DirectX 12 texture upload buffer를 매핑하지 못했습니다", result, true));
		}
		auto* destination = static_cast<std::uint8_t*>(mappedData) + layout.Offset;
		const UINT sourcePitch = width * 4;
		for (UINT row = 0; row < rowCount; row++)
			std::memcpy(destination + layout.Footprint.RowPitch * row, pixels + sourcePitch * row, sourcePitch);
		uploadBuffer->Unmap(0, nullptr);
		const auto uploadResult = uploadContext.UploadTexture(
			sourceDevice, texture.resource.Get(), uploadBuffer.Get(), layout);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		texture.width = width;
		texture.height = height;
		texture.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		return {};
	}

	GraphicsResult<std::optional<Dx12Texture>> Dx12TextureCache::Load(
		const Dx12Device& sourceDevice, const std::filesystem::path& texturePath) {
		const TextureKey key{ TextureKind::File, texturePath };
		if (const auto texture = FindCachedTexture(key))
			return std::optional{ *texture };
		const auto [pixels, width, height, components] = LoadImageRgba(texturePath);
		if (!pixels)
			return std::optional<Dx12Texture>{};
		Dx12Texture texture;
		texture.hasAlpha = components == 4;
		const auto uploadResult = UploadRgbaPixels(
			sourceDevice, pixels.get(), width, height, texture);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		textures.emplace(key, texture);
		return std::optional{ texture };
	}

	GraphicsResult<Dx12Texture> Dx12TextureCache::CreateWhiteTexture(const Dx12Device& sourceDevice) {
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		constexpr unsigned char white[] = { 255, 255, 255, 255 };
		Dx12Texture texture;
		texture.hasAlpha = false;
		const auto uploadResult = UploadRgbaPixels(sourceDevice, white, 1, 1, texture);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		textures.emplace(key, texture);
		return texture;
	}
}
